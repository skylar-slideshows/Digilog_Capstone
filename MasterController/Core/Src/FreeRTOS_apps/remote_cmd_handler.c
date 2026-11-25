/**
  **********************************************************************************
  * REMOTE COMMAND HANDLER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/remote_cmd_handler.c
  * @brief The master's side of the remote app protocol, over USB.
  *
  * The remote app talks to the master's USB serial port exactly as it talked to a bucket's
  * UART: commands framed as %...%, each recognised one answered with "$ack <command>^".
  *
  * Everything that touches TinyUSB happens in the USB task: TinyUSB calls tud_cdc_rx_cb()
  * there when the app sends, the bytes go through the bucket's %...% framing straight away,
  * and the acknowledgement is written from there too. No other task calls into TinyUSB.
  *
  * Knob (ud) and switch (sw) commands are handed to relay_task, which sends them to the
  * bucket that owns the channel over the master SPI, so an SPI transfer never holds up USB.
  * A command is only acknowledged once relay_task has it. If its queue were ever full, the
  * command gets no acknowledgement and the app sends it again: everything the app waits on
  * still arrives, and a live knob position it doesn't wait on is replaced by the next one.
  *
  * Forwarding: "<verb> <channel><rest>", channel 0-31, goes to bucket channel/4 as
  * "%<verb> <channel%4><rest>%", everything after the channel passed on unchanged:
  *   "ud 5 25 40000" -> bucket 1 gets "%ud 1 25 40000%"
  *   "sw 9 0 10"     -> bucket 2 gets "%sw 1 0 10%"
  * chnm, sel, sett and md are acknowledged, as the bucket does, but not forwarded yet.
  **********************************************************************************
*/

#include "FreeRTOS_apps/remote_cmd_handler.h"
#include "hardware_drivers/bucket_spi.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "cmsis_os2.h"
#include "tusb.h"
#include <string.h>

// commands waiting for relay_task: about 200 knob updates (each costs its length + 4 bytes)
#define RELAY_BUFFER_BYTES 4096

_Static_assert(CHANNELS_PER_BUCKET <= 10, "a bucket's channel number is sent as one digit");
_Static_assert(RELAY_BUFFER_BYTES >= CMD_MAX + 4, "the relay buffer must hold the longest command");


/*=============================== COMMAND DEFNS ================================*/
static const char *const VERBS[CMD_COUNT] = {
    "chnm", // CMD_CHNM  channel name
    "ud",   // CMD_UD    update DAC
    "sw",   // CMD_SW    switch
    "sel",  // CMD_SEL   select
    "sett", // CMD_SETT  setting
    "md",   // CMD_MD    mode
};


/*=============================== STATE ================================*/

// receive (only the USB task touches these)
static char rx[CMD_MAX + 1];
static uint16_t rx_len;
static bool in_frame;

// the acknowledgement being sent: "$ack " + command + "^"
static char ack[CMD_MAX + 6];

// diagnostic values
static volatile uint32_t stat_bytes;
static volatile uint32_t stat_frames;
static volatile uint32_t stat_unknown;
static volatile uint32_t stat_ack_dropped;
static volatile uint32_t stat_queue_full;   // written by the USB task
static volatile uint32_t stat_relayed;      // written by relay_task
static volatile uint32_t stat_relay_failed;

// commands to forward, handed from the USB task to relay_task
static uint8_t               relay_storage[RELAY_BUFFER_BYTES + 1];
static StaticMessageBuffer_t relay_buffer;
static MessageBufferHandle_t relay_frames;
static osThreadId_t          relay_thread;
static uint32_t              relay_stack[512];
static StaticTask_t          relay_tcb;


/*=============================== TRANSMIT (USB task) ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL send_ack : command text -> void
  Queues "$ack <command>^" on the USB serial port in one write, so it can't interleave
  with anything else sent there. Dropped (and counted) if it doesn't fit: the USB task
  must never wait on the app.
 ----------------------------------------------------------------------------------
*/
static void send_ack (const char *text, uint16_t len)
{
    // 5 for "$ack ", 1 for the closing '^'
    const uint32_t need = (uint32_t)len + 6U;
    if (tud_cdc_write_available() < need)
    {
        stat_ack_dropped++;
        return;
    }

    ack[0] = ACK_OPEN;
    memcpy(&ack[1], "ack ", 4);
    memcpy(&ack[5], text, len);
    ack[5 + len] = ACK_CLOSE;

    tud_cdc_write(ack, need);
    tud_cdc_write_flush();
}


/*=============================== PARSING (USB task) ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL verb_of : command string -> cmd_verb_t
  Takes the first whitespace-delimited word and matches it whole with the defined command verbs
 ----------------------------------------------------------------------------------
*/
static cmd_verb_t verb_of (const char *s)
{
    uint16_t len = 0;
    while (s[len] && s[len] != ' ' && s[len] != '\t') len++;
    if (len == 0) return CMD_UNKNOWN;

    for (uint8_t i = 0; i < CMD_COUNT; i++)
    {
        if (strlen(VERBS[i]) == len && strncmp(s, VERBS[i], len) == 0) return (cmd_verb_t)i;
    }
    return CMD_UNKNOWN;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL frame_complete : verb -> void
  A whole command has arrived: hand it to relay_task if it's for a bucket, then acknowledge
 ----------------------------------------------------------------------------------
*/
static void frame_complete (cmd_verb_t verb)
{
    stat_frames++;

    if (verb == CMD_UD || verb == CMD_SW)
    {
        if (relay_frames == NULL || xMessageBufferSend(relay_frames, rx, rx_len, 0) != rx_len)
        {
            stat_queue_full++;
            return; // no acknowledgement, so the app sends it again
        }
    }

    send_ack(rx, rx_len);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL feed_byte : byte -> void
  The bucket's UART receive state machine, one byte at a time
 ----------------------------------------------------------------------------------
*/
static void feed_byte (char c)
{
    stat_bytes++;

    if (c == CMD_OPEN && !in_frame)
    {
        in_frame = true; // start a frame
        rx_len   = 0;
    }
    else if (in_frame)
    {
        if (c == CMD_CLOSE)
        {
            rx[rx_len] = '\0';
            const cmd_verb_t verb = verb_of(rx);
            if (verb == CMD_UNKNOWN)
            {
                // Not a command, so we are out of step (a lost byte): treat this % as the next frame's opening
                if (rx_len) stat_unknown++;
                rx_len = 0;
            }
            else
            {
                in_frame = false;
                frame_complete(verb);
            }
        }
        else if (rx_len >= CMD_MAX)
        {
            // Over-long frame, abandon and wait for the next opening %, so
            // one bad frame cannot desynchronise the % interpretation.
            in_frame = false;
            rx_len   = 0;
        }
        else
        {
            rx[rx_len++] = c;
        }
    }
    // else: junk between frames, discard
}


/*=============================== USB SERIAL (TinyUSB callbacks, in the USB task) ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief TinyUSB callback: the remote app has sent bytes. Runs in the USB task.
 ----------------------------------------------------------------------------------
*/
void tud_cdc_rx_cb (uint8_t itf)
{
    (void)itf;
    char chunk[32];
    uint32_t n;

    while ((n = tud_cdc_read(chunk, sizeof(chunk))) > 0)
    {
        for (uint32_t i = 0; i < n; i++) feed_byte(chunk[i]);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief TinyUSB callback: the remote app opened (dtr) or closed the port.
  Opening starts a fresh session, so any half-received frame from before is forgotten.
 ----------------------------------------------------------------------------------
*/
void tud_cdc_line_state_cb (uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)rts;
    if (dtr)
    {
        in_frame = false;
        rx_len   = 0;
    }
}


/*=============================== FORWARDING TO THE BUCKETS (relay_task) ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL relay_frame : command string -> bool
  Renumbers the channel for its bucket and sends the command there. False if the
  channel is missing or out of range, or the SPI transfer failed.
 ----------------------------------------------------------------------------------
*/
static bool relay_frame (const char *frame)
{
    static char out[CMD_MAX + 3];

    uint16_t verb_len = 0;
    while (frame[verb_len] && frame[verb_len] != ' ' && frame[verb_len] != '\t') verb_len++;

    // the console channel: the whole number after the verb
    const char *p = frame + verb_len;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return false;

    uint32_t channel = 0;
    uint8_t digits   = 0;
    while (*p >= '0' && *p <= '9')
    {
        if (++digits > 3) return false; // nothing that long is a channel
        channel = channel * 10U + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != '\0' && *p != ' ' && *p != '\t') return false; // e.g. "5x"
    if (channel >= CONSOLE_CHANNELS) return false;

    const uint8_t bucket = (uint8_t)(channel / CHANNELS_PER_BUCKET);
    const uint8_t local  = (uint8_t)(channel % CHANNELS_PER_BUCKET); // one digit: a bucket has 4

    // rebuild: '%' verb ' ' local-channel rest '%'
    const size_t rest_len = strlen(p); // e.g. " 25 40000", sent as it came
    if (verb_len + rest_len + 4U > sizeof(out)) return false;

    uint16_t n = 0;
    out[n++] = CMD_OPEN;
    memcpy(&out[n], frame, verb_len);
    n = (uint16_t)(n + verb_len);
    out[n++] = ' ';
    out[n++] = (char)('0' + local);
    memcpy(&out[n], p, rest_len);
    n = (uint16_t)(n + rest_len);
    out[n++] = CMD_CLOSE;

    return bucket_spi_send(bucket, (const uint8_t *)out, n) == 0;
}


static void relay_task (void *argument)
{
    static char frame[CMD_MAX + 1];
    (void)argument;

    for (;;)
    {
        frame[xMessageBufferReceive(relay_frames, frame, CMD_MAX, portMAX_DELAY)] = '\0';

        if (relay_frame(frame)) stat_relayed++;
        else                    stat_relay_failed++;
    }
}


/*=============================== PUBLIC FUNCTIONS ================================*/

void remote_cmd_counts (remote_cmd_stats_t *out)
{
    if (out == NULL) return;

    out->bytes        = stat_bytes;
    out->frames       = stat_frames;
    out->unknown      = stat_unknown;
    out->ack_dropped  = stat_ack_dropped;
    out->queue_full   = stat_queue_full;
    out->relayed      = stat_relayed;
    out->relay_failed = stat_relay_failed;
}


void remote_cmd_init (void)
{
    if (relay_thread != NULL) return; // already running

    rx_len   = 0;
    in_frame = false;

    relay_frames = xMessageBufferCreateStatic(sizeof(relay_storage) - 1, relay_storage, &relay_buffer);
    static const osThreadAttr_t relay_attr = {
        .name = "remote_relay", .priority = osPriorityNormal,
        .stack_mem = relay_stack, .stack_size = sizeof(relay_stack),
        .cb_mem = &relay_tcb,     .cb_size = sizeof(relay_tcb),
    };
    relay_thread = osThreadNew(relay_task, NULL, &relay_attr);
}
