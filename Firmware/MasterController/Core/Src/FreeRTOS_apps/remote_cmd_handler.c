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
  * Knob (ud), switch (sw) and channel-name (chnm) commands are handed to relay_task, which
  * records the value in the mix state (mix/mix_state.c: what "save mix" writes) and queues the
  * command for the bucket that owns the channel; the bucket link (bucket_link.c) sends it in that
  * bucket's next SPI window. A bucket that isn't connected is skipped (the value is still
  * recorded). When a connected bucket's queue is full, relay_task waits for room, so the app's
  * own acknowledgements slow it to the speed the buckets take commands, and nothing is dropped.
  * Names are also kept, and sent again whenever a bucket (re)starts and says hello.
  * A command is only acknowledged once relay_task has it. If its queue were ever full, the
  * command gets no acknowledgement and the app sends it again: everything the app waits on
  * still arrives, and a live knob position it doesn't wait on is replaced by the next one.
  *
  * Forwarding: "<verb> <channel><rest>", channel 0-31, goes to bucket channel/4 as
  * "%<verb> <channel%4><rest>%", everything after the channel passed on unchanged:
  *   "ud 5 25 40000" -> bucket 1 gets "%ud 1 25 40000%"
  *   "sw 9 0 10"     -> bucket 2 gets "%sw 1 0 10%"
  *   "chnm 6 Kick In" -> bucket 1 gets "%chnm 2 Kick In%"
  * The other way, the master tells the app what changes on the console, as the app's own
  * "$ud ...^" / "$sw ...^" messages: the buckets' reports and automation playing back
  * (remote_report, queued, written by the USB task), so its controls move with the console.
  *
  * time sets the master's clock (mix titles, file dates). sel, sett and md are acknowledged,
  * as the bucket does, but not forwarded yet. When the app opens the port, every bucket is
  * asked to report everything again (%dump%), which also brings the mix state up to date.
  **********************************************************************************
*/

#include "FreeRTOS_apps/remote_cmd_handler.h"
#include "FreeRTOS_apps/bucket_link.h"
#include "mix/mix_state.h"
#include "mix/mix_file.h" // the clock
#include <stdio.h>
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "cmsis_os2.h"
#include "tusb.h"
#include <string.h>

// commands waiting for relay_task: about 100 knob updates (each costs its length + 4 bytes)
#define RELAY_BUFFER_BYTES 2048

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
    "time", // CMD_TIME  the app's clock
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
static volatile uint32_t stat_skipped;      // for a bucket that isn't connected

// commands to forward, handed from the USB task to relay_task
static uint8_t               relay_storage[RELAY_BUFFER_BYTES + 1];
static StaticMessageBuffer_t relay_buffer;
static MessageBufferHandle_t relay_frames;
static osThreadId_t          relay_thread;
static uint32_t              relay_stack[512];
static StaticTask_t          relay_tcb;

// reports for the app (console -> app), written by the USB task
#define REPORT_BUFFER_BYTES 1024
static uint8_t               report_storage[REPORT_BUFFER_BYTES + 1];
static StaticMessageBuffer_t report_buffer;
static MessageBufferHandle_t report_frames;
static volatile bool         s_app_open; // the app has the port open (DTR)


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
  @brief INTERNAL set_clock : "time 2026-11-24 14:05:33" -> void
  Read by hand rather than with sscanf: this runs in the USB task, whose stack is small.
 ----------------------------------------------------------------------------------
*/
static void set_clock (const char *cmd)
{
    static const char SEP[6] = {' ', '-', '-', ' ', ':', ':'}; // before each of the six numbers
    uint16_t v[6];
    const char *p = cmd + 4; // after "time"
    for (uint8_t i = 0; i < 6U; i++)
    {
        if (*p != SEP[i]) return;
        p++;
        if (*p < '0' || *p > '9') return;
        uint32_t n = 0;
        while (*p >= '0' && *p <= '9' && n < 10000U) n = n * 10U + (uint32_t)(*p++ - '0');
        v[i] = (uint16_t)n;
    }
    const mix_time_t t = {v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3], (uint8_t)v[4], (uint8_t)v[5]};
    mix_clock_set(&t); // checks the ranges
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

    if (verb == CMD_TIME) set_clock(rx);

    if (verb == CMD_UD || verb == CMD_SW || verb == CMD_CHNM)
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
    s_app_open = dtr;
    if (!dtr && report_frames != NULL) xMessageBufferReset(report_frames); // nobody to tell
    if (dtr)
    {
        in_frame = false;
        rx_len   = 0;
        bucket_link_dump_all(); // the app has just connected: every bucket reports everything again
    }
}


/*=============================== FORWARDING TO THE BUCKETS (relay_task) ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL relay_frame : command string -> bool
  Renumbers the channel for its bucket and queues the command for it (names are kept by the
  bucket link, which sends them). False if the channel is missing or out of range, or the
  bucket's queue is full.
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

    if (verb_len == 4U && strncmp(frame, "chnm", 4) == 0)
    {
        bucket_link_name((uint16_t)channel, p); // kept, and sent in the bucket's next window
        return true;
    }

    const uint8_t bucket = (uint8_t)(channel / CHANNELS_PER_BUCKET);
    const uint8_t local  = (uint8_t)(channel % CHANNELS_PER_BUCKET); // one digit: a bucket has 4

    // the value goes into the mix state whether or not the bucket is there
    {
        unsigned id = 0, value = 0;
        char bits[3] = {0};
        if (verb_len == 2U && strncmp(frame, "ud", 2) == 0 && sscanf(p, "%u %u", &id, &value) == 2 && value <= 65535U)
        {
            mix_state_set_knob((uint16_t)channel, (uint8_t)id, (uint16_t)value);
        }
        else if (verb_len == 2U && strncmp(frame, "sw", 2) == 0 && sscanf(p, "%u %2[01]", &id, bits) == 2)
        {
            // "0"/"1"; the input type as "00" line, "01" mic, "10" hi-z
            const uint8_t v = bits[1] ? (uint8_t)((bits[0] - '0') * 2 + (bits[1] - '0')) : (uint8_t)(bits[0] - '0');
            mix_state_set_switch((uint16_t)channel, (uint8_t)id, v);
        }
    }

    // a bucket that isn't connected: skipped (it reports its own state when it starts)
    if (!bucket_link_listening(bucket))
    {
        stat_skipped++;
        return true;
    }

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

    // wait for room rather than drop it: the app's acknowledgements then slow it to the buckets' pace
    return bucket_link_send_wait(bucket, out, n, 500U);
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

void remote_report (const char *text, uint16_t len)
{
    if (!s_app_open || report_frames == NULL || text == NULL || len == 0U) return;
    (void)xMessageBufferSend(report_frames, text, len, 0); // full: dropped (the next change brings it up to date)
}


void remote_cmd_usb_service (void)
{
    if (report_frames == NULL) return;
    char msg[64];
    bool wrote = false;
    for (;;)
    {
        const size_t next = xMessageBufferNextLengthBytes(report_frames);
        if (next == 0U) break;
        if (next > sizeof(msg)) { (void)xMessageBufferReceive(report_frames, msg, sizeof(msg), 0); continue; } // can't happen
        if (tud_cdc_write_available() < next) break; // the rest when the app has read some
        const size_t n = xMessageBufferReceive(report_frames, msg, sizeof(msg), 0);
        tud_cdc_write(msg, (uint32_t)n);
        wrote = true;
    }
    if (wrote) tud_cdc_write_flush();
}


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
    report_frames = xMessageBufferCreateStatic(sizeof(report_storage) - 1, report_storage, &report_buffer);
    static const osThreadAttr_t relay_attr = {
        .name = "remote_relay", .priority = osPriorityNormal,
        .stack_mem = relay_stack, .stack_size = sizeof(relay_stack),
        .cb_mem = &relay_tcb,     .cb_size = sizeof(relay_tcb),
    };
    relay_thread = osThreadNew(relay_task, NULL, &relay_attr);
}
