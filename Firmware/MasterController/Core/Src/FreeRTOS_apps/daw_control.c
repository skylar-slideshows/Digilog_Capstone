/**
  **********************************************************************************
  * DAW CONTROL - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/daw_control.c
  * @brief Mackie Control (MCU) transport and timecode, over USB MIDI cable 0.
  *
  * DAW -> console. TinyUSB calls tud_midi_rx_cb() (in the USB task) when MIDI arrives;
  * that wakes daw_task, which reads every waiting packet:
  *   note on/off, channel 1:  the DAW setting an LED. Play 0x5E, stop 0x5D, record 0x5F;
  *                            velocity 0x7F on, 0x01 flashing, 0x00 off. SMPTE 0x71 and
  *                            beats 0x72 say what the timecode display is showing.
  *   CC 0x40-0x49, channel 1: one timecode digit each, 0x40 the rightmost. The DAW only
  *                            sends digits that changed.
  * Once the packets are read, a change of transport state is reported on USART2 as one
  * line of text, with where the playhead is, e.g.  "DAW play @ 00.01.23.15 (SMPTE)".
  *
  * Console -> DAW. A button press is note on with velocity 0x7F, its release the same
  * note with velocity 0 (Mackie Control's convention).
  *
  * Packets on cable 1 ("Timecode", where the DAW can send MIDI Time Code) are read and
  * discarded for now, so they can't fill the receive buffer and hold up cable 0.
  **********************************************************************************
*/

#include "FreeRTOS_apps/daw_control.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os2.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef LOG_UART_HANDLE;


/*=============================== MACKIE CONTROL ================================*/

#define MCU_CABLE       0    // USB MIDI cable ("Mackie Control" in usb_descriptors.c)
#define MCU_NOTE_ON     0x90 // note on, MIDI channel 1: buttons and LEDs
#define MCU_NOTE_OFF    0x80 // some DAWs turn LEDs off with note off
#define MCU_CC          0xB0 // control change, MIDI channel 1: timecode digits

// USB-MIDI code index numbers: the low nibble of a packet's first byte
#define CIN_NOTE_OFF    0x8
#define CIN_NOTE_ON     0x9
#define CIN_CC          0xB

// note numbers
#define MCU_STOP        0x5D
#define MCU_PLAY        0x5E
#define MCU_RECORD      0x5F
#define MCU_LED_SMPTE   0x71
#define MCU_LED_BEATS   0x72

// velocities
#define MCU_PRESS       0x7F
#define MCU_RELEASE     0x00

// timecode display
#define MCU_TC_FIRST_CC 0x40 // rightmost digit; the leftmost is 0x49
#define MCU_TC_DIGITS   10

#define TAP_MS          20 // how long daw_control_tap holds a button down

typedef enum
{
    LED_OFF = 0,
    LED_FLASH,
    LED_ON
} led_t;

typedef struct
{
    uint8_t modifier; // held around the note, or DAW_NO_MODIFIER
    uint8_t note;
} daw_key_t;

static const daw_key_t KEYS[DAW_BUTTON_COUNT] = {
    [DAW_PLAY]     = {DAW_NO_MODIFIER, MCU_PLAY},
    [DAW_STOP]     = {DAW_NO_MODIFIER, MCU_STOP},
    [DAW_RECORD]   = {DAW_NO_MODIFIER, MCU_RECORD},
    [DAW_TO_START] = {DAW_TO_START_MODIFIER, DAW_TO_START_NOTE},
    [DAW_TO_END]   = {DAW_TO_END_MODIFIER, DAW_TO_END_NOTE},
};


/*=============================== STATE ================================*/

// what the DAW has said so far (only daw_task touches these)
static led_t led_play, led_stop, led_record, led_smpte, led_beats;
static char  tc_digits[MCU_TC_DIGITS]; // [0] is the rightmost digit
static bool  tc_dots[MCU_TC_DIGITS];   // a dot drawn after that digit
static bool  tc_valid;

// the last transport state written to USART2
static daw_transport_t reported_state;
static bool            reported_armed;

// copies for other tasks, updated after each batch of packets so they're never half-way through a change
static daw_timecode_t  published_tc;
static daw_transport_t published_state;
static bool            published_armed;

// task
static TaskHandle_t      s_task;
static uint32_t          task_stack[512];
static StaticTask_t      task_tcb;
static SemaphoreHandle_t s_send_lock; // keeps a modifier and its note together
static StaticSemaphore_t s_send_lock_mem;


/*=============================== TIMECODE ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL set_digit : position (0 = rightmost), CC value -> void
  The value's low six bits are a character in Mackie's display set (0x30-0x39 are the
  digits); bit 6 lights the dot after it.
 ----------------------------------------------------------------------------------
*/
static void set_digit (uint8_t position, uint8_t value)
{
    const uint8_t c = value & 0x3F;
    tc_digits[position] = (char)(c < 0x20 ? c + 0x40 : c); // 0x00-0x1F are '@', 'A'-'Z' ...; 0x20-0x3F are ASCII
    tc_dots[position]   = (value & 0x40) != 0;
    tc_valid            = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL build_timecode : out -> void
  The display as text (left to right, with the DAW's dots) and as four numbers
 ----------------------------------------------------------------------------------
*/
static void build_timecode (daw_timecode_t *out)
{
    static const uint8_t GROUP_LEFT[4] = {9, 6, 4, 2}; // leftmost digit of each group
    static const uint8_t GROUP_LEN[4]  = {3, 2, 2, 3};

    memset(out, 0, sizeof(*out));
    out->valid = tc_valid;
    out->mode  = (led_smpte != LED_OFF) ? DAW_TIME_SMPTE : (led_beats != LED_OFF) ? DAW_TIME_BEATS : DAW_TIME_UNKNOWN;

    char text[2 * MCU_TC_DIGITS + 1];
    uint8_t n = 0;
    for (int8_t i = MCU_TC_DIGITS - 1; i >= 0; i--)
    {
        text[n++] = tc_digits[i];
        if (tc_dots[i]) text[n++] = '.';
    }
    text[n] = '\0';

    // without the spaces the DAW pads with at either end
    uint8_t start = 0;
    while (text[start] == ' ') start++;
    while (n > start && text[n - 1] == ' ') text[--n] = '\0';
    strncpy(out->text, &text[start], sizeof(out->text) - 1);

    for (uint8_t g = 0; g < 4; g++)
    {
        uint16_t value = 0;
        for (uint8_t k = 0; k < GROUP_LEN[g]; k++)
        {
            const char c = tc_digits[GROUP_LEFT[g] - k];
            if (c >= '0' && c <= '9') value = (uint16_t)(value * 10U + (uint16_t)(c - '0'));
        }
        out->field[g] = value;
    }
}


/*=============================== TRANSPORT ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL set_led : note, velocity -> void
 ----------------------------------------------------------------------------------
*/
static void set_led (uint8_t note, uint8_t velocity)
{
    const led_t state = (velocity == 0) ? LED_OFF : (velocity == 1) ? LED_FLASH : LED_ON;

    switch (note)
    {
    case MCU_PLAY:      led_play   = state; break;
    case MCU_STOP:      led_stop   = state; break;
    case MCU_RECORD:    led_record = state; break;
    case MCU_LED_SMPTE: led_smpte  = state; break;
    case MCU_LED_BEATS: led_beats  = state; break;
    default:            break; // not one of ours
    }
}


static daw_transport_t transport_state (void)
{
    // Playing is the play LED; the stop LED isn't needed (not every DAW lights it)
    if (led_play == LED_FLASH) return DAW_PAUSED;
    if (led_play == LED_ON) return (led_record != LED_OFF) ? DAW_RECORDING : DAW_PLAYING;
    return DAW_STOPPED;
}


static bool record_armed (daw_transport_t state)
{
    return led_record != LED_OFF && (state == DAW_STOPPED || state == DAW_PAUSED);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL log_line : event -> void
  "DAW <event> @ <timecode> (<mode>)" on USART2
 ----------------------------------------------------------------------------------
*/
static void log_line (const char *event)
{
    static const char *const MODE[] = {"", " (SMPTE)", " (beats)"};
    daw_timecode_t tc;
    build_timecode(&tc);

    char line[96];
    const int n = snprintf(line,
                           sizeof(line),
                           "DAW %s @ %s%s\r\n",
                           event,
                           tc.valid ? tc.text : "--",
                           MODE[tc.mode]);
    if (n > 0) HAL_UART_Transmit(&LOG_UART_HANDLE, (uint8_t *)line, (uint16_t)n, LOG_UART_TIMEOUT_MS);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL report_changes : void -> void
  One line per change of transport state, however many LED messages it took. Repeats
  (a DAW re-sending all its LEDs) produce nothing.
 ----------------------------------------------------------------------------------
*/
static void report_changes (void)
{
    const daw_transport_t state = transport_state();
    const bool armed            = record_armed(state);

    if (state != reported_state)
    {
        if (reported_state == DAW_RECORDING && state == DAW_PLAYING) log_line("record off");
        else
        {
            static const char *const EVENT[] = {
                [DAW_STOPPED]   = "stop",
                [DAW_PLAYING]   = "play",
                [DAW_PAUSED]    = "pause",
                [DAW_RECORDING] = "record",
            };
            log_line(EVENT[state]);
        }
        reported_state = state;
    }

    if (armed != reported_armed)
    {
        if (armed) log_line("record armed");
        else if (state == DAW_STOPPED || state == DAW_PAUSED) log_line("record disarmed");
        // else the transport started rolling with record on, already reported as "record"
        reported_armed = armed;
    }
}


/*=============================== RECEIVE ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL handle_packet : USB-MIDI packet -> void
  packet[0] = cable << 4 | code index, packet[1..3] = the MIDI message
 ----------------------------------------------------------------------------------
*/
static void handle_packet (const uint8_t packet[4])
{
    if ((packet[0] >> 4) != MCU_CABLE) return; // cable 1 (MIDI Time Code): not handled yet

    const uint8_t cin = packet[0] & 0x0F;

    if (cin == CIN_NOTE_ON && packet[1] == MCU_NOTE_ON) set_led(packet[2], packet[3]);
    else if (cin == CIN_NOTE_OFF && packet[1] == MCU_NOTE_OFF) set_led(packet[2], 0);
    else if (cin == CIN_CC && packet[1] == MCU_CC && packet[2] >= MCU_TC_FIRST_CC &&
             packet[2] < MCU_TC_FIRST_CC + MCU_TC_DIGITS)
    {
        set_digit((uint8_t)(packet[2] - MCU_TC_FIRST_CC), packet[3]);
    }
    // everything else Mackie sends (faders, meters, scribble strips, SysEx) is ignored
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL process_pending : void -> void
  Reads every waiting packet, then publishes the result and reports any transport change
 ----------------------------------------------------------------------------------
*/
static void process_pending (void)
{
    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) handle_packet(packet);

    daw_timecode_t tc;
    build_timecode(&tc);
    const daw_transport_t state = transport_state();

    taskENTER_CRITICAL();
    published_tc    = tc;
    published_state = state;
    published_armed = record_armed(state);
    taskEXIT_CRITICAL();

    report_changes();
}


static void daw_task (void *argument)
{
    (void)argument;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // woken by tud_midi_rx_cb()
        process_pending();
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief TinyUSB callback: MIDI has arrived from the computer. Runs in the USB task.
 ----------------------------------------------------------------------------------
*/
void tud_midi_rx_cb (uint8_t itf)
{
    (void)itf;
    if (s_task != NULL) xTaskNotifyGive(s_task);
}


/*=============================== TRANSMIT ================================*/

static bool send_note (uint8_t note, uint8_t velocity)
{
    const uint8_t packet[4] = {(uint8_t)((MCU_CABLE << 4) | CIN_NOTE_ON), MCU_NOTE_ON, note, velocity};
    return tud_midi_packet_write(packet);
}


/*=============================== PUBLIC FUNCTIONS ================================*/

bool daw_control_button (daw_button_t button, bool pressed)
{
    // nothing is queued while unplugged; otherwise old presses would go out on connect
    if (button >= DAW_BUTTON_COUNT || !tud_midi_mounted()) return false;

    const daw_key_t key = KEYS[button];
    bool ok         = true;

    xSemaphoreTake(s_send_lock, portMAX_DELAY);
    if (pressed)
    {
        if (key.modifier != DAW_NO_MODIFIER) ok = send_note(key.modifier, MCU_PRESS) && ok;
        ok = send_note(key.note, MCU_PRESS) && ok;
    }
    else
    {
        ok = send_note(key.note, MCU_RELEASE) && ok;
        if (key.modifier != DAW_NO_MODIFIER) ok = send_note(key.modifier, MCU_RELEASE) && ok;
    }
    xSemaphoreGive(s_send_lock);

    return ok;
}


bool daw_control_tap (daw_button_t button)
{
    const bool down = daw_control_button(button, true);
    vTaskDelay(pdMS_TO_TICKS(TAP_MS));
    const bool up = daw_control_button(button, false);
    return down && up;
}


daw_transport_t daw_control_transport (bool *armed)
{
    taskENTER_CRITICAL();
    const daw_transport_t state = published_state;
    const bool a                = published_armed;
    taskEXIT_CRITICAL();

    if (armed) *armed = a;
    return state;
}


void daw_control_timecode (daw_timecode_t *out)
{
    if (out == NULL) return;

    taskENTER_CRITICAL();
    *out = published_tc;
    taskEXIT_CRITICAL();
}


void daw_control_init (void)
{
    memset(tc_digits, ' ', sizeof(tc_digits));
    build_timecode(&published_tc);

    s_send_lock = xSemaphoreCreateMutexStatic(&s_send_lock_mem);

    static const osThreadAttr_t attr = {
        .name = "daw_control", .priority = osPriorityNormal,
        .stack_mem = task_stack, .stack_size = sizeof(task_stack),
        .cb_mem = &task_tcb,     .cb_size = sizeof(task_tcb),
    };
    s_task = (TaskHandle_t)osThreadNew(daw_task, NULL, &attr);
}
