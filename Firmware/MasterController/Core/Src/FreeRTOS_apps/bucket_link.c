/**
  **********************************************************************************
  * BUCKET LINK - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/bucket_link.c
  * @brief See bucket_link.h.
  **********************************************************************************
*/

#include "FreeRTOS_apps/bucket_link.h"
#include "hardware_drivers/bucket_spi.h"
#include "mix/mix_state.h"
#include "mix/automation.h"
#include "FreeRTOS_apps/remote_cmd_handler.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include "semphr.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

#define QUEUE_BYTES   256  // commands waiting for one bucket's windows (a dozen; the app resends if full)
#define CTRL_IDS      27   // ud ids 0-26 (26 = fader)

_Static_assert(CHANNELS_PER_BUCKET <= 10, "a bucket's channel number is sent as one digit");
_Static_assert(BUCKET_XCHG_BYTES >= 32, "a window must hold an acknowledgement and a command");

typedef struct
{
    volatile bucket_status_t status;
    TickType_t last_valid;     // last valid packet back
    TickType_t last_window;    // last exchange
    bool ever_valid;
    uint8_t last_seq;          // last report packet taken; acknowledged in every window (0: none)
    bool want_base;            // %base% goes out in the next window
    volatile uint8_t names;    // bit per local channel: its name still to send
    volatile bool want_dump;   // %dump% in the next window
    bucket_link_stats_t stats;
} link_t;

static link_t s_link[BUCKET_COUNT];
static char s_names[CONSOLE_CHANNELS][CHANNEL_NAME_MAX + 1]; // from the app; "" for none

static MessageBufferHandle_t s_queue[BUCKET_COUNT];
static uint8_t s_queue_mem[BUCKET_COUNT][QUEUE_BYTES + 1];
static StaticMessageBuffer_t s_queue_cb[BUCKET_COUNT];
static SemaphoreHandle_t s_send_lock; // bucket_link_send from more than one task (the app relay, the screen)
static StaticSemaphore_t s_send_lock_mem;


static uint32_t s_stack[512];
static StaticTask_t s_tcb;
static TickType_t s_started;


/*=============================== PACKETS FROM A BUCKET ================================*/

static bool read_number (const char **p, const char *end, uint32_t max, uint32_t *out)
{
    const char *s = *p;
    while (s < end && *s == ' ') s++;
    if (s >= end || *s < '0' || *s > '9') return false;
    uint32_t v = 0;
    while (s < end && *s >= '0' && *s <= '9')
    {
        v = v * 10U + (uint32_t)(*s - '0');
        if (v > max) return false;
        s++;
    }
    *p = s;
    *out = v;
    return true;
}

static int hex_digit (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * One report from a bucket, e.g. "ud 5 25 40000" (console channel). While AUTO REC records it's a
 * move at the time of this window (automation.c); otherwise a change to the mix. Either way the app
 * is told, so its controls follow the console.
 */
static void take_report (uint8_t bucket, const char *r, const char *end, TickType_t when)
{
    const char *p;
    uint32_t ch, id, value;
    char app[32];
    if (end - r > 3 && strncmp(r, "ud ", 3) == 0)
    {
        p = r + 3;
        if (read_number(&p, end, CONSOLE_CHANNELS - 1U, &ch) && read_number(&p, end, CTRL_IDS - 1U, &id) &&
            read_number(&p, end, 65535U, &value))
        {
            if (!automation_record((uint16_t)ch, (uint8_t)id, (uint16_t)value, (uint32_t)when, AUTOMATION_REPORT_LATENCY_MS))
            {
                mix_state_set_knob((uint16_t)ch, (uint8_t)id, (uint16_t)value);
            }
            remote_report(app, (uint16_t)snprintf(app, sizeof(app), "$ud %u %u %u^", (unsigned)ch, (unsigned)id, (unsigned)value));
        }
    }
    else if (end - r > 3 && strncmp(r, "sw ", 3) == 0)
    {
        p = r + 3;
        if (!read_number(&p, end, CONSOLE_CHANNELS - 1U, &ch) || !read_number(&p, end, MIX_SWITCHES - 1U, &id)) return;
        while (p < end && *p == ' ') p++;
        if (p >= end || (*p != '0' && *p != '1')) return;
        // "0"/"1"; the input type as two binary digits: "00" line, "01" mic, "10" hi-z
        uint8_t v = (uint8_t)(*p - '0');
        const bool two = p + 1 < end && (p[1] == '0' || p[1] == '1');
        if (two) v = (uint8_t)(v * 2U + (uint8_t)(p[1] - '0'));
        if (!automation_record((uint16_t)ch, AUTO_LANE_SW(id), v, (uint32_t)when, AUTOMATION_REPORT_LATENCY_MS))
        {
            mix_state_set_switch((uint16_t)ch, (uint8_t)id, v);
        }
        remote_report(app, (uint16_t)snprintf(app, sizeof(app), "$sw %u %u %.*s^", (unsigned)ch, (unsigned)id, two ? 2 : 1, p));
    }
    else if (end - r >= 5 && strncmp(r, "sel ", 4) == 0)
    {
        p = r + 4;
        if (*p == '-') mix_state_set_bucket_select(bucket, -1); // none of its channels
        else if (read_number(&p, end, CONSOLE_CHANNELS - 1U, &ch)) mix_state_set_bucket_select(bucket, (int16_t)ch);
    }
}

/**
 * Looks at what a bucket sent back in a window. Returns true for a valid packet (and acts on it);
 * false for nothing (no bucket, or a bucket with nothing loaded) or anything that didn't check out.
 */
static bool take_packet (uint8_t bucket, const uint8_t *rx, uint16_t len, TickType_t now)
{
    link_t *l = &s_link[bucket];
    const char *buf = (const char *)rx;
    const char *start = memchr(buf, '$', len);
    if (start == NULL) return false;
    const char *end = buf + len;

    // $<seq>:<reports>#<xx>^
    const char *p = start + 1;
    uint32_t seq;
    if (!read_number(&p, end, 99U, &seq) || p >= end || *p != ':') { l->stats.bad++; return false; }
    const char *body = p + 1;
    const char *hash = memchr(body, '#', (size_t)(end - body));
    if (hash == NULL || end - hash < 4 || hash[3] != '^') { l->stats.bad++; return false; }
    const int hi = hex_digit(hash[1]), lo = hex_digit(hash[2]);
    uint8_t x = 0;
    for (const char *c = start + 1; c < hash; c++) x ^= (uint8_t)*c;
    if (hi < 0 || lo < 0 || x != (uint8_t)(hi * 16 + lo)) { l->stats.bad++; return false; }

    l->stats.packets++;
    if (seq == 0U)
    {
        if (hash - body == 2 && strncmp(body, "hi", 2) == 0) // hello: on, but doesn't know its channels
        {
            l->stats.hellos++;
            l->want_base = true;
            l->names = (uint8_t)((1U << CHANNELS_PER_BUCKET) - 1U); // and its channels' names
            l->last_seq = 0;                                       // it has started counting again
            l->status = BUCKET_CONNECTING;
        }
        else
        {
            l->status = BUCKET_PRESENT; // keep-alive: there, nothing to report
        }
        return true;
    }

    l->status = BUCKET_PRESENT;
    if ((uint8_t)seq == l->last_seq)
    {
        l->stats.repeats++; // already taken; it sends it until our acknowledgement arrives
        return true;
    }
    l->last_seq = (uint8_t)seq;
    l->stats.reports++;
    for (const char *r = body; r < hash;)
    {
        const char *semi = memchr(r, ';', (size_t)(hash - r));
        const char *r_end = semi ? semi : hash;
        take_report(bucket, r, r_end, now);
        r = r_end + 1;
    }
    return true;
}


/*=============================== WINDOWS TO A BUCKET ================================*/

/** Appends a command to the window if it fits whole; a command is never split across windows */
static bool put (uint8_t *tx, uint16_t *n, const char *frame, uint16_t len)
{
    if (len == 0U || *n + len > BUCKET_XCHG_BYTES) return false;
    memcpy(&tx[*n], frame, len);
    *n = (uint16_t)(*n + len);
    return true;
}

static void build_window (uint8_t bucket, uint8_t *tx)
{
    link_t *l = &s_link[bucket];
    char cmd[CHANNEL_NAME_MAX + 16];
    uint16_t n = 0;

    // acknowledgement first: the last report packet taken from this bucket
    put(tx, &n, cmd, (uint16_t)snprintf(cmd, sizeof(cmd), "%%ack %u%%", (unsigned)l->last_seq));

    if (l->want_base) // it said hello: which channels it holds
    {
        if (put(tx, &n, cmd, (uint16_t)snprintf(cmd, sizeof(cmd), "%%base %u%%", (unsigned)(bucket * CHANNELS_PER_BUCKET + 1U))))
        {
            l->want_base = false;
        }
    }
    for (uint8_t local = 0; local < CHANNELS_PER_BUCKET; local++) // names, while they fit
    {
        if (((l->names >> local) & 1U) == 0U) continue;
        char name[CHANNEL_NAME_MAX + 1];
        taskENTER_CRITICAL();
        memcpy(name, s_names[bucket * CHANNELS_PER_BUCKET + local], sizeof(name));
        taskEXIT_CRITICAL();
        const int len = name[0] ? snprintf(cmd, sizeof(cmd), "%%chnm %u %s%%", (unsigned)local, name)
                                : snprintf(cmd, sizeof(cmd), "%%chnm %u%%", (unsigned)local);
        if (!put(tx, &n, cmd, (uint16_t)len)) break;
        taskENTER_CRITICAL();
        l->names &= (uint8_t)~(1U << local);
        taskEXIT_CRITICAL();
    }
    if (l->want_dump && put(tx, &n, "%dump%", 6)) l->want_dump = false;

    // the app's commands for this bucket, whole ones while they fit
    for (;;)
    {
        const size_t next = xMessageBufferNextLengthBytes(s_queue[bucket]);
        if (next == 0U || n + next > BUCKET_XCHG_BYTES) break;
        n = (uint16_t)(n + xMessageBufferReceive(s_queue[bucket], &tx[n], next, 0));
    }
    memset(&tx[n], 0xFF, BUCKET_XCHG_BYTES - n); // between frames: ignored by the bucket
}

static void update_status (uint8_t bucket, bool valid, TickType_t now)
{
    link_t *l = &s_link[bucket];
    if (valid)
    {
        l->last_valid = now;
        l->ever_valid = true;
        return; // take_packet set it
    }
    const TickType_t quiet = now - (l->ever_valid ? l->last_valid : s_started);
    if (quiet >= pdMS_TO_TICKS(BUCKET_GIVE_UP_MS))
    {
        if (l->status != BUCKET_GONE) printf("\r\nBucket %u (CS_Bucket%u): no answer, taken as unplugged (still asked)\n", (unsigned)bucket, (unsigned)bucket);
        l->status = BUCKET_GONE;
    }
    else if (l->ever_valid && quiet >= pdMS_TO_TICKS(BUCKET_LOST_MS))
    {
        l->status = BUCKET_LOST;
    }
}

/** One window with every bucket due one */
static void link_round (void)
{
    static uint8_t tx[BUCKET_XCHG_BYTES], rx[BUCKET_XCHG_BYTES];
    for (uint8_t b = 0; b < BUCKET_COUNT; b++)
    {
        link_t *l = &s_link[b];
        const TickType_t now = xTaskGetTickCount();
        // one taken as unplugged is still asked, just less often
        if (l->status == BUCKET_GONE && (TickType_t)(now - l->last_window) < pdMS_TO_TICKS(BUCKET_RETRY_MS)) continue;
        l->last_window = now;

        const bucket_status_t before = l->status;
        build_window(b, tx);
        memset(rx, 0, sizeof(rx));
        const bool sent = bucket_spi_exchange(b, tx, rx, BUCKET_XCHG_BYTES) == 0;
        l->stats.windows++;
        update_status(b, sent && take_packet(b, rx, BUCKET_XCHG_BYTES, now), now);
        if (l->status != before && (l->status == BUCKET_PRESENT || l->status == BUCKET_CONNECTING))
        {
            printf("\r\nBucket %u (CS_Bucket%u): %s\n", (unsigned)b, (unsigned)b,
                   l->status == BUCKET_PRESENT ? "present" : "said hello, sending its channel numbers");
        }
    }
}

static void link_task (void *argument)
{
    (void)argument;
    s_started = xTaskGetTickCount();
    TickType_t wake = s_started;
    for (;;)
    {
        automation_tick(); // AUTO REC passes, and moves played to the buckets for where the playhead is
        link_round();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(BUCKET_POLL_MS));
    }
}


/*=============================== PUBLIC ================================*/

void bucket_link_start (void)
{
    s_send_lock = xSemaphoreCreateMutexStatic(&s_send_lock_mem);
    for (uint8_t b = 0; b < BUCKET_COUNT; b++)
    {
        s_link[b] = (link_t){.status = BUCKET_ASKING};
        s_queue[b] = xMessageBufferCreateStatic(QUEUE_BYTES, s_queue_mem[b], &s_queue_cb[b]);
    }
    static const osThreadAttr_t attr = {
        .name = "bucket_link",
        // below the panel's controls and LEDs (BelowNormal), so an exchange never delays a knob read;
        // above the display (Low)
        .priority = osPriorityLow7,
        .stack_mem = s_stack,
        .stack_size = sizeof(s_stack),
        .cb_mem = &s_tcb,
        .cb_size = sizeof(s_tcb),
    };
    osThreadNew(link_task, NULL, &attr);
}

bucket_status_t bucket_link_status (uint8_t bucket)
{
    return bucket < BUCKET_COUNT ? s_link[bucket].status : BUCKET_GONE;
}

bool bucket_link_send (uint8_t bucket, const char *frame, uint16_t len)
{
    if (bucket >= BUCKET_COUNT || frame == NULL || len == 0U || len > BUCKET_XCHG_BYTES - 12U) return false;
    // a message buffer takes one writer at a time
    xSemaphoreTake(s_send_lock, portMAX_DELAY);
    const bool ok = xMessageBufferSend(s_queue[bucket], frame, len, 0) == len;
    xSemaphoreGive(s_send_lock);
    return ok;
}

bool bucket_link_send_wait (uint8_t bucket, const char *frame, uint16_t len, uint32_t timeout_ms)
{
    const TickType_t start = xTaskGetTickCount();
    for (;;)
    {
        if (bucket_link_send(bucket, frame, len)) return true;
        if (!bucket_link_listening(bucket)) return false; // it went away
        if ((TickType_t)(xTaskGetTickCount() - start) >= pdMS_TO_TICKS(timeout_ms)) return false;
        vTaskDelay(pdMS_TO_TICKS(BUCKET_POLL_MS)); // a window or two drains it
    }
}

bool bucket_link_listening (uint8_t bucket)
{
    if (bucket >= BUCKET_COUNT) return false;
    const bucket_status_t st = s_link[bucket].status;
    return st == BUCKET_PRESENT || st == BUCKET_CONNECTING || st == BUCKET_LOST;
}

void bucket_link_name_get (uint16_t channel, char out[CHANNEL_NAME_MAX + 1])
{
    if (out == NULL) return;
    if (channel >= CONSOLE_CHANNELS)
    {
        out[0] = '\0';
        return;
    }
    taskENTER_CRITICAL();
    memcpy(out, s_names[channel], CHANNEL_NAME_MAX + 1);
    taskEXIT_CRITICAL();
}

void bucket_link_name (uint16_t channel, const char *text)
{
    if (channel >= CONSOLE_CHANNELS || text == NULL) return;
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    while (len > 0U && (text[len - 1U] == ' ' || text[len - 1U] == '\t')) len--;
    if (len > CHANNEL_NAME_MAX) len = CHANNEL_NAME_MAX;
    char name[CHANNEL_NAME_MAX + 1];
    for (size_t i = 0; i < len; i++)
    {
        const unsigned char c = (unsigned char)text[i];
        name[i] = (c >= 0x20U && c < 0x7FU && c != '%') ? (char)c : '?'; // plain ASCII, and never a frame mark
    }
    name[len] = '\0';

    link_t *l = &s_link[channel / CHANNELS_PER_BUCKET];
    if (strcmp(s_names[channel], name) != 0) mix_state_changed(); // for autosave
    taskENTER_CRITICAL();
    memcpy(s_names[channel], name, len + 1U);
    l->names |= (uint8_t)(1U << (channel % CHANNELS_PER_BUCKET));
    taskEXIT_CRITICAL();
}

void bucket_link_dump_all (void)
{
    for (uint8_t b = 0; b < BUCKET_COUNT; b++) s_link[b].want_dump = true;
}

void bucket_link_counts (uint8_t bucket, bucket_link_stats_t *out)
{
    if (bucket >= BUCKET_COUNT || out == NULL) return;
    *out = s_link[bucket].stats;
}
