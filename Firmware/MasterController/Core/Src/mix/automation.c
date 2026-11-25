/**
  **********************************************************************************
  * AUTOMATION - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/automation.c
  * @brief See automation.h.
  *
  * Who does what. The engine (the bucket link's task, every 5 ms) decides; the storage task
  * (automation_service) reads and writes the card. They meet in these, under s_lock:
  *   - s_rec[2]: the moves being recorded. One fills; the other is spilled to the pass file
  *     (state SPILL: the storage task owns it until it's empty again).
  *   - s_win[2]: moves about to play, read ahead from the store (state ASKED -> READY).
  *   - a seek: "values at this time?" (SEEK_ASKED -> SEEK_DONE): the storage task replays the
  *     store up to it, and fills the first window after it.
  *   - a merge: a pass is over (MERGE_ASKED -> MERGE_RUNNING -> idle): its moves go into the store.
  * s_gen changes whenever the store is rebuilt, so nothing read from the old one is used.
  **********************************************************************************
*/

#include "mix/automation.h"
#include "mix/mix_store.h"
#include "FreeRTOS_apps/bucket_link.h"
#include "FreeRTOS_apps/daw_control.h"
#include "FreeRTOS_apps/remote_cmd_handler.h"
#include "control_integration/control_interface.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

#define REC_N          AUTOMATION_REC_EVENTS
#define WIN_N          AUTOMATION_WINDOW_EVENTS
#define SEEK_JITTER_MS 100U // while rolling, the playhead seeming to step back less than this is jitter
#define MAX_TOUCHED    64   // controls a single pass can record
#define CHUNK          32

_Static_assert(AUTO_LANES <= 64, "a channel's lanes are kept as bits of a uint64_t");
_Static_assert(MIX_MASTER <= AUTO_LANES, "the master section fits in one channel's lanes");

enum { BUF_FILLING = 0, BUF_SPILL };                        // s_rec_state
enum { WIN_FREE = 0, WIN_ASKED, WIN_READY };                // s_win[].state
enum { SEEK_IDLE = 0, SEEK_ASKED, SEEK_DONE };              // s_seek_state
enum { MERGE_IDLE = 0, MERGE_ASKED, MERGE_RUNNING };        // s_merge_state

typedef struct
{
    store_ev_t ev[WIN_N];
    uint32_t first;
    uint16_t n;
    uint32_t gen;
    volatile uint8_t state;
} window_t;

static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_mem;

static volatile auto_rec_t s_rec;
static volatile bool s_full;
static volatile uint32_t s_recorded; // moves recorded since power-up

// lanes
static uint64_t s_used[AUTO_CHANNELS];    // lanes with moves
static uint64_t s_dirty[AUTO_CHANNELS];   // to send
static uint64_t s_touched[AUTO_CHANNELS]; // touched in the pass that's on (not played back)
static uint64_t s_pending[AUTO_CHANNELS]; // touched in a pass still being written in (not played back either)
static uint16_t s_out[AUTO_CHANNELS][AUTO_LANES];  // each lane's value at the playhead
static uint16_t s_sent[AUTO_CHANNELS][AUTO_LANES]; // what was last sent

// the store, as the storage task last published it
static volatile uint32_t s_store_n, s_gen, s_pass_spilled;

// playback
static uint32_t s_idx;  // moves at or before s_pos
static uint32_t s_pos;  // where the playhead was at the last tick
static bool s_have_pos, s_need_seek = true, s_was_rolling;
static window_t s_win[2];
static uint8_t s_win_cur; // the window last played from

// seek
static volatile uint8_t s_seek_state;
static uint32_t s_seek_pos, s_seek_first, s_seek_gen;
static uint16_t s_seek_val[AUTO_CHANNELS][AUTO_LANES];
static uint64_t s_seek_has[AUTO_CHANNELS];

// recording
static store_ev_t s_rec_buf[2][REC_N];
static uint16_t s_rec_n[2];
static volatile uint8_t s_rec_state[2];
static uint8_t s_rec_cur;
static store_touch_t s_touch[MAX_TOUCHED];
static uint32_t s_touch_last[MAX_TOUCHED];   // its last move's time
static int16_t s_touch_idx[MAX_TOUCHED];     // ...and where it is in s_rec_buf[s_rec_cur] (-1: spilled)
static uint8_t s_touch_n;
static uint32_t s_pass_last_t; // the latest move's time this pass: moves stay in time order

// a finished pass, for the storage task
static volatile uint8_t s_merge_state;
static store_touch_t s_merge_touch[MAX_TOUCHED];
static uint8_t s_merge_n, s_merge_tail;
static uint32_t s_merge_end;

static store_ev_t s_chunk[CHUNK]; // the storage task's reads


/*=============================== HELPERS ================================*/

static bool bit (const uint64_t *bits, uint16_t ch, uint8_t lane)
{
    return ((bits[ch] >> lane) & 1U) != 0U;
}

static bool lane_held (uint16_t ch, uint8_t lane) // touched: not played back now
{
    return bit(s_touched, ch, lane) || bit(s_pending, ch, lane);
}

/** A knob or fader (recorded at most every AUTOMATION_MIN_GAP_MS), not a switch */
static bool continuous (uint16_t ch, uint8_t lane)
{
    return ch < CONSOLE_CHANNELS ? lane < MIX_KNOBS : lane < AUX_RETURNS;
}

static uint16_t static_value (uint16_t ch, uint8_t lane)
{
    if (ch == AUTO_MASTER_CH) return mix_state_get_master(lane);
    mix_channel_t c;
    mix_state_get(ch, &c);
    return lane < MIX_KNOBS ? c.knob[lane] : c.sw[lane - MIX_KNOBS];
}

static void set_out (uint16_t ch, uint8_t lane, uint16_t v)
{
    s_out[ch][lane] = v;
    if (s_sent[ch][lane] != v) s_dirty[ch] |= 1ULL << lane;
}

static window_t *window_with (uint32_t index)
{
    for (uint8_t w = 0; w < 2; w++)
    {
        window_t *win = &s_win[w];
        if (win->state == WIN_READY && win->gen == s_gen && index >= win->first && index < win->first + win->n) return win;
    }
    return NULL;
}

/** Asks for the store's moves from `first` in a window that isn't being played from */
static void ask_window (uint32_t first)
{
    for (uint8_t w = 0; w < 2; w++)
    {
        if (s_win[w].state != WIN_FREE && s_win[w].first == first && s_win[w].gen == s_gen) return; // asked already
    }
    window_t *other = &s_win[s_win_cur ^ 1U];
    if (other->state == WIN_ASKED) return; // one read at a time
    other->first = first;
    other->n = 0;
    other->gen = s_gen;
    other->state = WIN_ASKED;
    mix_file_wake();
}

/** Plays the moves between the last playhead and pos, from the windows */
static void advance (uint32_t pos)
{
    while (s_idx < s_store_n)
    {
        window_t *w = window_with(s_idx);
        if (w == NULL)
        {
            ask_window(s_idx); // not read yet: carry on once it is
            return;
        }
        s_win_cur = (uint8_t)(w - s_win);
        const store_ev_t *e = &w->ev[s_idx - w->first];
        if (e->t > pos) break;
        if (e->ch < AUTO_CHANNELS && e->lane < AUTO_LANES && !lane_held(e->ch, e->lane)) set_out(e->ch, e->lane, e->value);
        s_idx++;
    }
    // read ahead: past half of this window, the next one
    const window_t *w = &s_win[s_win_cur];
    if (w->state == WIN_READY && w->gen == s_gen && s_idx >= w->first + w->n / 2U && w->first + w->n < s_store_n)
    {
        ask_window(w->first + w->n);
    }
}

static void ask_seek (uint32_t pos)
{
    s_seek_pos = pos;
    s_seek_state = SEEK_ASKED;
    s_need_seek = false;
    mix_file_wake();
}

/** Sends what changed: to the buckets that are connected (and the app), and to the master's aux returns */
static void send_dirty (void)
{
    for (uint16_t ch = 0; ch < AUTO_CHANNELS; ch++)
    {
        if (s_dirty[ch] == 0U) continue;
        if (ch == AUTO_MASTER_CH)
        {
            for (uint8_t lane = 0; lane < MIX_MASTER; lane++)
            {
                if (!bit(s_dirty, ch, lane)) continue;
                const uint16_t v = s_out[ch][lane];
                if (lane < AUX_RETURNS) control_set_aux_level(lane, v);
                else control_set_aux_to_mix((uint8_t)(lane - AUX_RETURNS), v != 0U);
                s_sent[ch][lane] = v;
            }
            s_dirty[ch] = 0;
            continue;
        }
        const uint8_t bucket = (uint8_t)(ch / CHANNELS_PER_BUCKET);
        if (!bucket_link_listening(bucket)) continue; // sent once it's back
        const unsigned local = ch % CHANNELS_PER_BUCKET;
        for (uint8_t lane = 0; lane < AUTO_LANES; lane++)
        {
            if (!bit(s_dirty, ch, lane)) continue;
            const uint16_t v = s_out[ch][lane];
            char cmd[32], app[32];
            int n, m;
            if (lane < MIX_KNOBS)
            {
                n = snprintf(cmd, sizeof(cmd), "%%ud %u %u %u%%", local, (unsigned)lane, (unsigned)v);
                m = snprintf(app, sizeof(app), "$ud %u %u %u^", (unsigned)ch, (unsigned)lane, (unsigned)v);
            }
            else if (lane == AUTO_LANE_SW(0)) // the input type: "00" line, "01" mic, "10" hi-z
            {
                n = snprintf(cmd, sizeof(cmd), "%%sw %u 0 %u%u%%", local, (unsigned)(v >> 1), (unsigned)(v & 1U));
                m = snprintf(app, sizeof(app), "$sw %u 0 %u%u^", (unsigned)ch, (unsigned)(v >> 1), (unsigned)(v & 1U));
            }
            else
            {
                n = snprintf(cmd, sizeof(cmd), "%%sw %u %u %u%%", local, (unsigned)(lane - MIX_KNOBS), (unsigned)(v != 0U));
                m = snprintf(app, sizeof(app), "$sw %u %u %u^", (unsigned)ch, (unsigned)(lane - MIX_KNOBS), (unsigned)(v != 0U));
            }
            if (!bucket_link_send(bucket, cmd, (uint16_t)n)) return; // its queue is full: the rest next round
            remote_report(app, (uint16_t)m);
            s_sent[ch][lane] = v;
            s_dirty[ch] &= ~(1ULL << lane);
        }
    }
}

static void start_pass (void)
{
    memset(s_touched, 0, sizeof(s_touched));
    s_touch_n = 0;
    s_full = false;
    s_pass_last_t = 0;
    s_rec = AUTO_REC_RECORDING;
    printf("\r\nAUTO REC: recording\n");
}

/** The pass is over at end_ms: its moves go to the storage task to be written into the store */
static void end_pass (uint32_t end_ms)
{
    if (s_touch_n > 0U)
    {
        memcpy(s_merge_touch, s_touch, s_touch_n * sizeof(store_touch_t));
        s_merge_n = s_touch_n;
        s_merge_end = end_ms;
        s_merge_tail = s_rec_cur;
        for (uint16_t ch = 0; ch < AUTO_CHANNELS; ch++) s_pending[ch] |= s_touched[ch];
        s_merge_state = MERGE_ASKED;
        s_rec_cur ^= 1U; // the tail buffer is the storage task's until the pass is in
        mix_file_wake();
    }
    memset(s_touched, 0, sizeof(s_touched));
    s_touch_n = 0;
    s_rec = AUTO_REC_OFF;
    printf("\r\nAUTO REC: pass over\n");
}


/*=============================== THE ENGINE (the bucket link's task) ================================*/

void automation_init (void)
{
    s_lock = xSemaphoreCreateMutexStatic(&s_lock_mem);
    s_rec = AUTO_REC_OFF;
    for (uint8_t k = 0; k < MAX_TOUCHED; k++) s_touch_idx[k] = -1;
}

void automation_tick (void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    // a seek the storage task has answered
    if (s_seek_state == SEEK_DONE)
    {
        if (s_seek_gen == s_gen)
        {
            for (uint16_t ch = 0; ch < AUTO_CHANNELS; ch++)
            {
                for (uint8_t lane = 0; lane < AUTO_LANES; lane++)
                {
                    if (!bit(s_used, ch, lane) || lane_held(ch, lane)) continue;
                    set_out(ch, lane, bit(s_seek_has, ch, lane) ? s_seek_val[ch][lane] : static_value(ch, lane));
                }
            }
            s_idx = s_seek_first;
            s_pos = s_seek_pos;
            s_have_pos = true;
            s_win_cur = 0;
        }
        else
        {
            s_need_seek = true; // the store changed meanwhile
        }
        s_seek_state = SEEK_IDLE;
    }

    const bool rolling = daw_control_rolling();
    uint32_t pos = 0;
    const bool have = daw_control_position_at((uint32_t)xTaskGetTickCount(), &pos);

    // AUTO REC: a pass while it's on and the transport rolls; a stop, or a jump back, ends it and turns it off
    const bool jumped_back = have && s_have_pos && pos + SEEK_JITTER_MS < s_pos;
    if (s_rec == AUTO_REC_ARMED && rolling && have && s_merge_state == MERGE_IDLE && s_rec_state[s_rec_cur] == BUF_FILLING)
    {
        start_pass();
    }
    else if (s_rec == AUTO_REC_RECORDING && (!rolling || jumped_back))
    {
        end_pass(s_pos);
    }
    else if (s_rec == AUTO_REC_ARMED && s_was_rolling && !rolling)
    {
        s_rec = AUTO_REC_OFF; // every stop turns it off
    }
    s_was_rolling = rolling;

    // a buffer three-quarters full goes to the card, oldest moves first, while the other fills
    if (s_rec == AUTO_REC_RECORDING && s_rec_n[s_rec_cur] >= (REC_N * 3U) / 4U)
    {
        const uint8_t other = s_rec_cur ^ 1U;
        if (s_rec_state[other] == BUF_FILLING && s_rec_n[other] == 0U)
        {
            s_rec_state[s_rec_cur] = BUF_SPILL;
            s_rec_cur = other;
            for (uint8_t k = 0; k < s_touch_n; k++) s_touch_idx[k] = -1;
            mix_file_wake();
        }
    }

    // playback
    if (have && s_seek_state == SEEK_IDLE)
    {
        const bool back = s_have_pos && pos < s_pos;
        const bool jitter = back && rolling && (s_pos - pos) < SEEK_JITTER_MS;
        bool any_used = false;
        for (uint16_t ch = 0; ch < AUTO_CHANNELS && !any_used; ch++) any_used = s_used[ch] != 0U;

        if (!any_used)
        {
            s_pos = pos; // nothing automated: just follow
            s_have_pos = true;
            s_need_seek = false;
        }
        else if (s_need_seek || !s_have_pos || (back && !jitter))
        {
            ask_seek(pos);
        }
        else if (!jitter)
        {
            advance(pos);
            s_pos = pos;
        }
    }
    send_dirty();
    xSemaphoreGive(s_lock);
}

bool automation_record (uint16_t ch, uint8_t lane, uint16_t value, uint32_t tick, uint32_t latency_ms)
{
    if (s_rec != AUTO_REC_RECORDING || ch >= AUTO_CHANNELS || lane >= AUTO_LANES) return false;
    uint32_t t;
    if (!daw_control_position_at(tick - pdMS_TO_TICKS(latency_ms), &t)) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_rec != AUTO_REC_RECORDING) // ended meanwhile: an ordinary change
    {
        xSemaphoreGive(s_lock);
        return false;
    }
    // the first touch in this pass: it stops playing back, and its old moves from here will go
    int16_t k = -1;
    for (uint8_t i = 0; i < s_touch_n && k < 0; i++) if (s_touch[i].ch == ch && s_touch[i].lane == lane) k = (int16_t)i;
    if (k < 0 && s_touch_n < MAX_TOUCHED)
    {
        k = (int16_t)s_touch_n++;
        s_touch[k] = (store_touch_t){(uint8_t)ch, lane, t};
        s_touch_last[k] = t;
        s_touch_idx[k] = -1;
        s_touched[ch] |= 1ULL << lane;
    }
    // what's there already: nothing to send
    s_out[ch][lane] = value;
    s_sent[ch][lane] = value;
    s_dirty[ch] &= ~(1ULL << lane);

    if (k >= 0)
    {
        store_ev_t *buf = s_rec_buf[s_rec_cur];
        if (continuous(ch, lane) && s_touch_idx[k] >= 0 && t - s_touch_last[k] < AUTOMATION_MIN_GAP_MS)
        {
            buf[s_touch_idx[k]].value = value; // still turning: its latest move takes the new value
        }
        else if (s_rec_state[s_rec_cur] == BUF_FILLING && s_rec_n[s_rec_cur] < REC_N)
        {
            // a bucket's move is stamped AUTOMATION_REPORT_LATENCY_MS back, the master's own aren't: so one
            // can come in a few ms "before" the last. The pass is kept in order (the card's store must be).
            if (t < s_pass_last_t) t = s_pass_last_t;
            s_pass_last_t = t;
            const uint16_t at = s_rec_n[s_rec_cur]++;
            buf[at] = (store_ev_t){t, (uint8_t)ch, lane, value};
            s_touch_idx[k] = (int16_t)at;
            s_touch_last[k] = t;
            s_used[ch] |= 1ULL << lane;
            s_recorded++;
        }
        else
        {
            s_full = true; // both buffers full: the card isn't keeping up (or there's no card)
        }
    }
    else
    {
        s_full = true; // more controls than one pass can record
    }
    xSemaphoreGive(s_lock);
    return true; // a move, not a change to the static mix
}

void automation_master_moved (uint8_t index, uint16_t value)
{
    if (index >= MIX_MASTER) return;
    if (automation_record(AUTO_MASTER_CH, index, value, (uint32_t)xTaskGetTickCount(), 0)) return;
    mix_state_set_master(index, value); // not recording: the mix's own value
}

void automation_rec_toggle (void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    switch (s_rec)
    {
        case AUTO_REC_OFF: s_rec = AUTO_REC_ARMED; break;
        case AUTO_REC_ARMED: s_rec = AUTO_REC_OFF; break;
        case AUTO_REC_RECORDING: end_pass(s_pos); break; // punched out: the pass ends here
    }
    xSemaphoreGive(s_lock);
}

auto_rec_t automation_rec_state (void)
{
    return s_rec;
}

auto_wait_t automation_waiting (void)
{
    if (s_rec != AUTO_REC_ARMED) return AUTO_WAIT_NONE;
    if (s_merge_state != MERGE_IDLE) return AUTO_WAIT_SAVING;
    uint32_t ms;
    if (!daw_control_position_at(0, &ms)) return AUTO_WAIT_TIMECODE;
    return daw_control_rolling() ? AUTO_WAIT_NONE : AUTO_WAIT_PLAY;
}

uint32_t automation_event_count (void)
{
    return s_store_n + s_pass_spilled + s_rec_n[0] + s_rec_n[1];
}

uint32_t automation_moves_recorded (void)
{
    return s_recorded;
}

bool automation_full (void)
{
    return s_full;
}

bool automation_busy (void)
{
    return s_rec == AUTO_REC_RECORDING || s_merge_state != MERGE_IDLE;
}


/*=============================== THE CARD (the storage task) ================================*/

void automation_store_loaded (const uint64_t used[AUTO_CHANNELS])
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_store_n = store_count();
    s_gen++;
    s_win[0].state = s_win[1].state = WIN_FREE;
    memcpy(s_used, used, sizeof(s_used));
    memset(s_pending, 0, sizeof(s_pending));
    // every automated control is sent once, where the playhead is
    for (uint16_t ch = 0; ch < AUTO_CHANNELS; ch++)
    {
        for (uint8_t lane = 0; lane < AUTO_LANES; lane++) s_sent[ch][lane] = (uint16_t)~static_value(ch, lane);
    }
    s_need_seek = true;
    xSemaphoreGive(s_lock);
}

void automation_service (void)
{
    // buffers to the pass file, oldest first
    for (uint8_t b = 0; b < 2; b++)
    {
        if (s_rec_state[b] != BUF_SPILL) continue;
        const uint16_t n = s_rec_n[b]; // the engine doesn't touch a SPILL buffer
        if (!store_pass_append(s_rec_buf[b], n)) s_full = true;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_pass_spilled += n;
        s_rec_n[b] = 0;
        s_rec_state[b] = BUF_FILLING;
        xSemaphoreGive(s_lock);
    }

    // a finished pass into the store
    if (s_merge_state == MERGE_ASKED)
    {
        s_merge_state = MERGE_RUNNING;
        const uint8_t tail = s_merge_tail;
        const bool ok = store_merge_pass(s_merge_touch, s_merge_n, s_merge_end, s_rec_buf[tail], s_rec_n[tail]);
        if (!ok) s_full = true;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_store_n = store_count();
        s_gen++;
        s_win[0].state = s_win[1].state = WIN_FREE;
        memset(s_pending, 0, sizeof(s_pending));
        s_pass_spilled = 0;
        s_rec_n[tail] = 0;
        s_need_seek = true; // the touched controls play back again, from the new store
        s_merge_state = MERGE_IDLE;
        xSemaphoreGive(s_lock);
        mix_state_changed();
        printf("\r\nAUTO REC: pass written, %lu moves in the mix\n", (unsigned long)s_store_n);
    }

    // each automated control's value at a time: the store replayed up to it
    if (s_seek_state == SEEK_ASKED)
    {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const uint32_t pos = s_seek_pos, gen = s_gen;
        xSemaphoreGive(s_lock);
        memset(s_seek_has, 0, sizeof(s_seek_has));
        uint32_t first = 0;
        bool ok = store_first_after(pos, &first);
        for (uint32_t i = 0; ok && i < first;)
        {
            uint16_t got = 0;
            ok = store_read(i, s_chunk, (uint16_t)((first - i) < CHUNK ? (first - i) : CHUNK), &got) && got > 0U;
            for (uint16_t j = 0; ok && j < got; j++)
            {
                const store_ev_t *e = &s_chunk[j];
                if (e->ch >= AUTO_CHANNELS || e->lane >= AUTO_LANES) continue;
                s_seek_val[e->ch][e->lane] = e->value;
                s_seek_has[e->ch] |= 1ULL << e->lane;
            }
            i += got;
        }
        // and the moves after it, ready to play
        uint16_t n = 0;
        if (ok) ok = store_read(first, s_win[0].ev, WIN_N, &n);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (!ok) first = s_store_n; // the card failed: play nothing more
        s_win[0].first = first;
        s_win[0].n = n;
        s_win[0].gen = gen;
        s_win[0].state = WIN_READY;
        s_win[1].state = WIN_FREE;
        s_seek_first = first;
        s_seek_gen = gen;
        s_seek_state = SEEK_DONE;
        xSemaphoreGive(s_lock);
    }

    // read-ahead
    for (uint8_t w = 0; w < 2; w++)
    {
        if (s_win[w].state != WIN_ASKED) continue;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const uint32_t first = s_win[w].first, gen = s_win[w].gen;
        xSemaphoreGive(s_lock);
        uint16_t n = 0;
        const bool ok = gen == s_gen && store_read(first, s_win[w].ev, WIN_N, &n);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_win[w].state == WIN_ASKED && s_win[w].first == first && s_win[w].gen == gen)
        {
            s_win[w].n = ok ? n : 0U;
            s_win[w].state = (ok && gen == s_gen) ? WIN_READY : WIN_FREE;
        }
        xSemaphoreGive(s_lock);
    }
}
