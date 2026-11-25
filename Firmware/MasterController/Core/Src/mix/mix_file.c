/**
  **********************************************************************************
  * MIX FILES - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_file.c
  * @brief See mix_file.h.
  *
  * The file (version 4; the remote app reads and writes the same JSON, keeping what it doesn't edit):
  *   { "version": 4, "current_channel": "Channel 1",
  *     "channels": { "Channel 1": { "name": "...",
  *                                  "knobs":   { "<tag>": 0-65535, ... },
  *                                  "toggles": { "<tag>": true/false, ... },
  *                                  "cycles":  { "input_src": 0/1/2 },
  *                                  "insert": null, "chain": null }, ... },
  *     "master": { "knobs":   { "aux_return_1": 0-65535, ... "aux_return_4" },
  *                 "toggles": { "aux_return_1_to_mix": true/false, ... } },
  *     "automation": [ [ms, channel, "<tag>", value], ... ] }
  * "channels" and "master" are the static mix: each control's value wherever it has no moves
  * (before its first). "automation" is every move in time order: ms from 00:00:00:00 on the DAW's
  * SMPTE timecode, the channel (1-16, or "master"), the control's tag (the app's, or the master's
  * as above) and its value (knobs 0-65535, switches 0/1, the input type 0/1/2).
  * Versions 2 and 3 still load (version 3's per-control automation isn't read: re-record it).
  * Loading reads the file a few bytes at a time and writes the moves straight to the card's
  * store (mix_store.c), so a mix of any size needs only small buffers.
  **********************************************************************************
*/

#include "mix/mix_file.h"
#include "mix/mix_state.h"
#include "mix/mix_store.h"
#include "mix/automation.h"
#include "FreeRTOS_apps/bucket_link.h"
#include "control_integration/control_interface.h"
#include "CONFIG.h"
#include "app_fatfs.h" // USERFatFs, USERFile, USERPath (CubeMX)
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os2.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MOUNT_RETRY_MS 2000U
#define AUTOSAVE_SUFFIX " (autosave)"
#define TMP_FILE "0:/AUTOSAVE.TMP"


/*=============================== THE APP'S NAMES FOR THE CONTROLS ================================*/
// from the app's CONFIG.yaml: knob tag by ud id, toggles by sw id; the master's own

static const char *const KNOB_TAGS[MIX_KNOBS] = {
    "input",      "send_1",     "send_1_pan",  "send_2",     "send_2_pan", "send_3",      "send_3_pan",
    "send_4",     "eq_1_gain",  "eq_1_freq",   "eq_2_gain",  "eq_2_freq",  "eq_2_q",      "eq_3_gain",
    "eq_3_freq",  "eq_3_q",     "eq_4_gain",   "eq_4_freq",  "comp_input", "comp_atk",    "comp_thresh",
    "comp_rel",   "comp_deess", "comp_ratio",  "comp_output", "pan",       "fader",
};

#define TOGGLE_SEL (-1) // "sel" is kept with the toggles but isn't a switch
static const struct { const char *tag; int8_t sw; } TOGGLES[] = {
    {"phase", 1}, {"48v", 2}, {"hpf", 3}, {"solo", 4}, {"mute", 5}, {"sel", TOGGLE_SEL}, {"insert", 6}, {"pre", 7},
};
#define TOGGLE_COUNT ((uint8_t)(sizeof(TOGGLES) / sizeof(TOGGLES[0])))

static int8_t knob_id (const char *tag)
{
    for (uint8_t i = 0; i < MIX_KNOBS; i++) if (strcmp(tag, KNOB_TAGS[i]) == 0) return (int8_t)i;
    return -1;
}

static int8_t toggle_index (const char *tag)
{
    for (uint8_t i = 0; i < TOGGLE_COUNT; i++) if (strcmp(tag, TOGGLES[i].tag) == 0) return (int8_t)i;
    return -1;
}

/** A channel lane's tag: a knob's, "input_src", or a toggle's */
static const char *lane_tag (uint8_t lane)
{
    if (lane < MIX_KNOBS) return KNOB_TAGS[lane];
    if (lane == AUTO_LANE_SW(0)) return "input_src";
    for (uint8_t i = 0; i < TOGGLE_COUNT; i++) if (TOGGLES[i].sw == (int8_t)(lane - MIX_KNOBS)) return TOGGLES[i].tag;
    return NULL;
}

/** A tag's lane on a channel; -1 if none */
static int8_t channel_lane (const char *tag)
{
    const int8_t k = knob_id(tag);
    if (k >= 0) return k;
    if (strcmp(tag, "input_src") == 0) return (int8_t)AUTO_LANE_SW(0);
    const int8_t t = toggle_index(tag);
    return (t >= 0 && TOGGLES[t].sw != TOGGLE_SEL) ? (int8_t)AUTO_LANE_SW(TOGGLES[t].sw) : -1;
}

/** The master's: "aux_return_2" -> 1, "aux_return_2_to_mix" -> AUX_RETURNS + 1; -1 if neither */
static int8_t master_lane (const char *tag)
{
    unsigned n = 0;
    char rest[12] = "";
    if (sscanf(tag, "aux_return_%u%11s", &n, rest) < 1 || n < 1U || n > AUX_RETURNS) return -1;
    if (rest[0] == '\0') return (int8_t)(n - 1U);
    if (strcmp(rest, "_to_mix") == 0) return (int8_t)(AUX_RETURNS + n - 1U);
    return -1;
}

static void master_tag (uint8_t lane, char *out, size_t size)
{
    if (lane < AUX_RETURNS) snprintf(out, size, "aux_return_%u", (unsigned)(lane + 1U));
    else snprintf(out, size, "aux_return_%u_to_mix", (unsigned)(lane - AUX_RETURNS + 1U));
}


/*=============================== CLOCK ================================*/

static volatile uint32_t s_base_s;     // seconds since 2000-01-01 00:00:00 at s_base_tick
static volatile TickType_t s_base_tick;
static volatile bool s_clock_set;

// days since 1970-01-01 (H. Hinnant's civil-calendar algorithms)
static int32_t days_from_civil (int32_t y, uint32_t m, uint32_t d)
{
    y -= (m <= 2U);
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = (uint32_t)(y - era * 400);
    const uint32_t doy = (153U * (m > 2U ? m - 3U : m + 9U) + 2U) / 5U + d - 1U;
    const uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

static void civil_from_days (int32_t z, mix_time_t *t)
{
    z += 719468;
    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = (uint32_t)(z - era * 146097);
    const uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
    const uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
    const uint32_t mp = (5U * doy + 2U) / 153U;
    const uint32_t d = doy - (153U * mp + 2U) / 5U + 1U;
    const uint32_t m = mp < 10U ? mp + 3U : mp - 9U;
    t->year = (uint16_t)((int32_t)yoe + era * 400 + (m <= 2U));
    t->month = (uint8_t)m;
    t->day = (uint8_t)d;
}

#define DAYS_1970_TO_2000 10957

void mix_clock_set (const mix_time_t *t)
{
    if (t == NULL || t->year < 2000U || t->year > 2099U || t->month < 1U || t->month > 12U || t->day < 1U ||
        t->day > 31U || t->hour > 23U || t->minute > 59U || t->second > 59U)
    {
        return;
    }
    const uint32_t days = (uint32_t)(days_from_civil(t->year, t->month, t->day) - DAYS_1970_TO_2000);
    const uint32_t s = days * 86400U + t->hour * 3600U + t->minute * 60U + t->second;
    taskENTER_CRITICAL();
    s_base_s = s;
    s_base_tick = xTaskGetTickCount();
    s_clock_set = true;
    taskEXIT_CRITICAL();
}

bool mix_clock_is_set (void)
{
    return s_clock_set;
}

void mix_clock_now (mix_time_t *t)
{
    if (t == NULL) return;
    taskENTER_CRITICAL();
    const uint32_t base = s_base_s;
    const TickType_t since = xTaskGetTickCount() - s_base_tick;
    taskEXIT_CRITICAL();
    const uint32_t s = base + (uint32_t)(since / configTICK_RATE_HZ);
    civil_from_days((int32_t)(s / 86400U) + DAYS_1970_TO_2000, t);
    const uint32_t sod = s % 86400U;
    t->hour = (uint8_t)(sod / 3600U);
    t->minute = (uint8_t)((sod / 60U) % 60U);
    t->second = (uint8_t)(sod % 60U);
}

uint32_t mix_clock_fattime (void)
{
    mix_time_t t;
    mix_clock_now(&t);
    if (!s_clock_set) t.year = 2026; // never set: at least a plausible year (FAT dates start at 1980)
    return ((uint32_t)(t.year - 1980U) << 25) | ((uint32_t)t.month << 21) | ((uint32_t)t.day << 16) |
           ((uint32_t)t.hour << 11) | ((uint32_t)t.minute << 5) | ((uint32_t)t.second >> 1);
}


/*=============================== THE CARD ================================*/

static bool s_mounted, s_store_ready;
static TickType_t s_last_mount_try;
static bool s_tried_mount;

static void path_of (char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s%s", USERPath, name); // "0:/" + "14-05-33.dgl"
}

/** After a failed operation: if the card itself failed, mount it again next time */
static void card_failed (FRESULT fr)
{
    if (fr == FR_DISK_ERR || fr == FR_NOT_READY || fr == FR_INT_ERR || fr == FR_INVALID_OBJECT || fr == FR_TIMEOUT)
    {
        s_mounted = false;
        (void)f_mount(NULL, USERPath, 0);
    }
}

static mix_result_t do_mount (void)
{
    s_tried_mount = true;
    s_last_mount_try = xTaskGetTickCount();
    if (s_mounted) return MIX_OK;
    const FRESULT fr = f_mount(&USERFatFs, USERPath, 1); // 1: mount now, so we know
    if (fr == FR_OK)
    {
        s_mounted = true;
        if (!s_store_ready) // the first time: the mix has no moves yet (the last session's files go)
        {
            static const uint64_t none[AUTO_CHANNELS] = {0};
            store_reset();
            s_store_ready = true;
            automation_store_loaded(none);
        }
        return MIX_OK;
    }
    (void)f_mount(NULL, USERPath, 0);
    if (fr == FR_NO_FILESYSTEM) return MIX_NOT_FORMATTED;
    if (fr == FR_NOT_READY || fr == FR_DISK_ERR) return MIX_NO_CARD;
    return MIX_DISK_ERROR;
}

static mix_result_t do_format (void)
{
    if (do_mount() != MIX_NOT_FORMATTED) return MIX_DISK_ERROR; // only an unformatted card
    static uint8_t work[_MAX_SS];
    if (f_mkfs(USERPath, FM_ANY, 0, work, sizeof(work)) != FR_OK) return MIX_DISK_ERROR;
    return do_mount();
}

static bool is_dgl (const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != NULL && (dot[1] == 'D' || dot[1] == 'd') && (dot[2] == 'G' || dot[2] == 'g') &&
           (dot[3] == 'L' || dot[3] == 'l') && dot[4] == '\0';
}

static uint16_t do_list (char names[][MIX_NAME_MAX + 1], uint16_t max)
{
    if (names == NULL || max == 0U || do_mount() != MIX_OK) return 0;
    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, USERPath);
    if (fr != FR_OK)
    {
        card_failed(fr);
        return 0;
    }
    uint16_t n = 0;
    while ((fr = f_readdir(&dir, &fno)) == FR_OK && fno.fname[0] != '\0' && n < max)
    {
        if (fno.fattrib & (AM_DIR | AM_HID | AM_SYS)) continue;
        if (!is_dgl(fno.fname) || strlen(fno.fname) > MIX_NAME_MAX) continue;
        strncpy(names[n], fno.fname, MIX_NAME_MAX);
        names[n][MIX_NAME_MAX] = '\0';
        n++;
    }
    (void)f_closedir(&dir);
    if (fr != FR_OK) card_failed(fr);

    for (uint16_t i = 1; i < n; i++) // sorted by name (insertion sort: a few dozen at most)
    {
        char key[MIX_NAME_MAX + 1];
        memcpy(key, names[i], sizeof(key));
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && strcmp(names[j], key) > 0)
        {
            memcpy(names[j + 1], names[j], sizeof(key));
            j--;
        }
        memcpy(names[j + 1], key, sizeof(key));
    }
    return n;
}

static mix_result_t do_delete (const char *name)
{
    const mix_result_t m = do_mount();
    if (m != MIX_OK) return m;
    char path[MIX_NAME_MAX + 8];
    path_of(path, sizeof(path), name);
    const FRESULT fr = f_unlink(path);
    if (fr == FR_OK) return MIX_OK;
    card_failed(fr);
    return (fr == FR_NO_FILE) ? MIX_NOT_FOUND : MIX_DISK_ERROR;
}

const char *mix_file_result_text (mix_result_t r)
{
    switch (r)
    {
        case MIX_OK:            return "OK";
        case MIX_NO_CARD:       return "No SD card";
        case MIX_NOT_FORMATTED: return "The SD card isn't formatted";
        case MIX_DISK_ERROR:    return "SD card error";
        case MIX_NOT_FOUND:     return "That mix isn't on the card";
        case MIX_BAD_FILE:      return "Not a mix this console can read";
        case MIX_EXISTS:        return "No free title for this second: try again";
        case MIX_BUSY:          return "AUTO REC is recording or writing its moves: wait for it";
        default:                return "?";
    }
}


/*=============================== SAVING (JSON out) ================================*/

typedef struct
{
    FIL *f;
    char buf[128];
    uint16_t n;
    bool err;
} writer_t;

static void w_flush (writer_t *w)
{
    if (w->n == 0U || w->err) { w->n = 0; return; }
    UINT done = 0;
    if (f_write(w->f, w->buf, w->n, &done) != FR_OK || done != w->n) w->err = true;
    w->n = 0;
}

static void w_char (writer_t *w, char c)
{
    if (w->n >= sizeof(w->buf)) w_flush(w);
    w->buf[w->n++] = c;
}

static void w_str (writer_t *w, const char *s)
{
    while (*s) w_char(w, *s++);
}

static void w_fmt (writer_t *w, const char *fmt, ...)
{
    char text[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    w_str(w, text);
}

static void w_json_string (writer_t *w, const char *s)
{
    w_char(w, '"');
    for (; *s; s++)
    {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { w_char(w, '\\'); w_char(w, (char)c); }
        else if (c < 0x20U) w_fmt(w, "\\u%04x", (unsigned)c);
        else w_char(w, (char)c);
    }
    w_char(w, '"');
}

static void write_channel (writer_t *w, uint16_t ch, bool last)
{
    mix_channel_t v;
    mix_state_get(ch, &v);
    char name[CHANNEL_NAME_MAX + 1];
    bucket_link_name_get(ch, name);
    char label[16];
    snprintf(label, sizeof(label), "Channel %u", (unsigned)(ch + 1U));

    w_str(w, "    ");
    w_json_string(w, label);
    w_str(w, ": {\n      \"name\": ");
    w_json_string(w, name[0] ? name : label); // unnamed: the app's own default name
    w_str(w, ",\n      \"knobs\": {\n");
    for (uint8_t id = 0; id < MIX_KNOBS; id++)
    {
        w_fmt(w, "        \"%s\": %u%s\n", KNOB_TAGS[id], (unsigned)v.knob[id], id + 1U < MIX_KNOBS ? "," : "");
    }
    w_str(w, "      },\n      \"toggles\": {\n");
    for (uint8_t i = 0; i < TOGGLE_COUNT; i++)
    {
        const bool on = TOGGLES[i].sw == TOGGLE_SEL ? v.sel : v.sw[TOGGLES[i].sw] != 0U;
        w_fmt(w, "        \"%s\": %s%s\n", TOGGLES[i].tag, on ? "true" : "false", i + 1U < TOGGLE_COUNT ? "," : "");
    }
    w_fmt(w, "      },\n      \"cycles\": {\n        \"input_src\": %u\n      },\n", (unsigned)v.sw[0]);
    w_fmt(w, "      \"insert\": null,\n      \"chain\": null\n    }%s\n", last ? "" : ",");
}

static void write_master (writer_t *w)
{
    char tag[24];
    w_str(w, "  \"master\": {\n    \"knobs\": {\n");
    for (uint8_t i = 0; i < AUX_RETURNS; i++)
    {
        master_tag(i, tag, sizeof(tag));
        w_fmt(w, "      \"%s\": %u%s\n", tag, (unsigned)mix_state_get_master(i), i + 1U < AUX_RETURNS ? "," : "");
    }
    w_str(w, "    },\n    \"toggles\": {\n");
    for (uint8_t i = 0; i < AUX_RETURNS; i++)
    {
        master_tag((uint8_t)(AUX_RETURNS + i), tag, sizeof(tag));
        w_fmt(w, "      \"%s\": %s%s\n", tag, mix_state_get_master((uint8_t)(AUX_RETURNS + i)) ? "true" : "false", i + 1U < AUX_RETURNS ? "," : "");
    }
    w_str(w, "    }\n  },\n");
}

/** Every move, in time order, straight from the card's store */
static void write_automation (writer_t *w)
{
    static store_ev_t chunk[32];
    w_str(w, "  \"automation\": [");
    const uint32_t n = store_count();
    bool first = true;
    for (uint32_t i = 0; i < n && !w->err;)
    {
        uint16_t got = 0;
        if (!store_read(i, chunk, 32, &got) || got == 0U) { w->err = true; break; }
        for (uint16_t j = 0; j < got; j++)
        {
            const store_ev_t *e = &chunk[j];
            char tag[24];
            if (e->ch == AUTO_MASTER_CH) master_tag(e->lane, tag, sizeof(tag));
            else
            {
                const char *t = lane_tag(e->lane);
                if (t == NULL) continue;
                snprintf(tag, sizeof(tag), "%s", t);
            }
            if (e->ch == AUTO_MASTER_CH) w_fmt(w, "%s\n    [%lu, \"master\", \"%s\", %u]", first ? "" : ",", (unsigned long)e->t, tag, (unsigned)e->value);
            else w_fmt(w, "%s\n    [%lu, %u, \"%s\", %u]", first ? "" : ",", (unsigned long)e->t, (unsigned)(e->ch + 1U), tag, (unsigned)e->value);
            first = false;
        }
        i += got;
    }
    w_str(w, first ? "]\n" : "\n  ]\n");
}

/** The whole mix to a file. new_only: fail if it exists; otherwise written alongside and renamed over it. */
static mix_result_t save_as (const char *name, bool new_only)
{
    char path[MIX_NAME_MAX + 8];
    path_of(path, sizeof(path), name);
    const char *target = new_only ? path : TMP_FILE;
    FRESULT fr = f_open(&USERFile, target, FA_WRITE | (new_only ? FA_CREATE_NEW : FA_CREATE_ALWAYS));
    if (fr == FR_EXIST) return MIX_EXISTS;
    if (fr != FR_OK)
    {
        card_failed(fr);
        return MIX_DISK_ERROR;
    }
    writer_t w = {.f = &USERFile};
    w_str(&w, "{\n  \"version\": 4,\n  \"current_channel\": \"Channel 1\",\n  \"channels\": {\n");
    for (uint16_t ch = 0; ch < CONSOLE_CHANNELS; ch++) write_channel(&w, ch, ch + 1U == CONSOLE_CHANNELS);
    w_str(&w, "  },\n");
    write_master(&w);
    write_automation(&w);
    w_str(&w, "}\n");
    w_flush(&w);
    fr = f_close(&USERFile);
    if (w.err || fr != FR_OK)
    {
        (void)f_unlink(target); // no half-written mixes
        card_failed(fr != FR_OK ? fr : FR_DISK_ERR);
        return MIX_DISK_ERROR;
    }
    if (!new_only)
    {
        (void)f_unlink(path);
        if (f_rename(TMP_FILE, path) != FR_OK) return MIX_DISK_ERROR;
    }
    return MIX_OK;
}

// what was last saved, for autosave
static uint32_t s_saved_changes, s_saved_moves;
static TickType_t s_saved_at;
static char s_auto_base[MIX_NAME_MAX + 1]; // the autosave's name, before " (autosave).dgl"
static volatile uint32_t s_autosaves;
static char s_autosave_name[MIX_NAME_MAX + 1];

static void mark_saved (void)
{
    s_saved_changes = mix_state_changes();
    s_saved_moves = automation_moves_recorded();
    s_saved_at = xTaskGetTickCount();
}

static mix_result_t do_save_new (char saved_name[MIX_NAME_MAX + 1])
{
    if (automation_busy()) return MIX_BUSY; // its moves aren't all in the store yet
    const mix_result_t m = do_mount();
    if (m != MIX_OK) return m;

    mix_time_t t;
    mix_clock_now(&t);
    for (uint8_t attempt = 0; attempt < 10U; attempt++)
    {
        char name[MIX_NAME_MAX + 1];
        if (attempt == 0U) snprintf(name, sizeof(name), "%02u-%02u-%02u.dgl", t.hour, t.minute, t.second);
        else snprintf(name, sizeof(name), "%02u-%02u-%02u (%u).dgl", t.hour, t.minute, t.second, (unsigned)attempt + 1U);
        const mix_result_t r = save_as(name, true);
        if (r == MIX_EXISTS) continue; // saved twice in a second: the next title
        if (r == MIX_OK)
        {
            if (saved_name != NULL) memcpy(saved_name, name, sizeof(name));
            mark_saved();
        }
        return r;
    }
    return MIX_EXISTS;
}

/** Autosave: AUTOSAVE_PERIOD_MS after the last save if the mix changed, or after AUTOSAVE_AFTER_MOVES moves */
static void autosave_check (void)
{
    if (!s_mounted || automation_busy()) return;
    const bool changed = mix_state_changes() != s_saved_changes;
    const uint32_t moves = automation_moves_recorded() - s_saved_moves;
    const bool period = changed && (TickType_t)(xTaskGetTickCount() - s_saved_at) >= pdMS_TO_TICKS(AUTOSAVE_PERIOD_MS);
    if (!period && moves < AUTOSAVE_AFTER_MOVES) return;

    if (s_auto_base[0] == '\0') // a mix started here: titled with the time of its first autosave
    {
        mix_time_t t;
        mix_clock_now(&t);
        snprintf(s_auto_base, sizeof(s_auto_base), "%02u-%02u-%02u", t.hour, t.minute, t.second);
    }
    char name[MIX_NAME_MAX + 1];
    snprintf(name, sizeof(name), "%.*s" AUTOSAVE_SUFFIX ".dgl", (int)(MIX_NAME_MAX - sizeof(AUTOSAVE_SUFFIX ".dgl") + 1), s_auto_base);
    if (save_as(name, false) == MIX_OK)
    {
        memcpy(s_autosave_name, name, sizeof(name));
        s_autosaves++;
        printf("\r\nAutosaved %s\n", name);
    }
    mark_saved(); // even if it failed: try again a period later, not every pass
}

uint32_t mix_file_autosaves (char name[MIX_NAME_MAX + 1])
{
    if (name != NULL) memcpy(name, s_autosave_name, MIX_NAME_MAX + 1);
    return s_autosaves;
}


/*=============================== LOADING (JSON in) ================================*/

#define KEY_MAX   24 // longest key kept (longer ones can't be ours)
#define DEPTH_MAX 8  // nesting this console reads (the file has 4)

typedef struct
{
    FIL *f;
    char buf[128];
    UINT len, pos;
    bool eof, err;
} reader_t;

/** What the parser fills for the channel being read */
typedef struct
{
    int16_t channel; //!< console channel of the object being read; -1 none
    mix_channel_t v; //!< the file's values (the bucket's defaults for anything missing)
    char name[CHANNEL_NAME_MAX + 1];
} pending_t;

typedef struct
{
    reader_t r;
    char path[DEPTH_MAX][KEY_MAX + 1]; // the key at each depth
    uint16_t elem[DEPTH_MAX + 1];      // the element being read in an array at each depth
    pending_t p;
    mix_load_report_t *report;
    uint16_t master[MIX_MASTER];       // "master"
    bool have_master;
    // "automation": the move being read, [ms, channel, "tag", value]
    uint32_t auto_t, last_t;
    int16_t auto_ch;
    char auto_tag[KEY_MAX + 1];
    bool store_ok;
    uint64_t used[AUTO_CHANNELS];
} loader_t;

static int r_peek (reader_t *r)
{
    if (r->pos >= r->len)
    {
        if (r->eof || r->err) return -1;
        if (f_read(r->f, r->buf, sizeof(r->buf), &r->len) != FR_OK) { r->err = true; return -1; }
        r->pos = 0;
        if (r->len == 0U) { r->eof = true; return -1; }
    }
    return (unsigned char)r->buf[r->pos];
}

static int r_next (reader_t *r)
{
    const int c = r_peek(r);
    if (c >= 0) r->pos++;
    return c;
}

static void r_ws (reader_t *r)
{
    int c;
    while ((c = r_peek(r)) == ' ' || c == '\t' || c == '\r' || c == '\n') r->pos++;
}

/** A JSON string (after its opening quote), kept up to size-1 characters; escapes simplified */
static bool r_string (reader_t *r, char *out, size_t size)
{
    size_t n = 0;
    for (;;)
    {
        int c = r_next(r);
        if (c < 0) return false;
        if (c == '"') break;
        if (c == '\\')
        {
            c = r_next(r);
            if (c < 0) return false;
            if (c == 'u') { for (uint8_t k = 0; k < 4U; k++) if (r_next(r) < 0) return false; c = '?'; }
            else if (c == 'n' || c == 't' || c == 'r' || c == 'b' || c == 'f') c = ' ';
        }
        if (n + 1U < size) out[n++] = (char)c;
    }
    out[n] = '\0';
    return true;
}

typedef enum { LEAF_STRING, LEAF_NUMBER, LEAF_BOOL, LEAF_NULL } leaf_t;

/** One move read: into the new store, if it's in time order */
static void take_move (loader_t *L, int32_t number)
{
    int8_t lane = -1;
    int32_t v = number < 0 ? 0 : number > 65535 ? 65535 : number;
    if (L->auto_ch == AUTO_MASTER_CH)
    {
        lane = master_lane(L->auto_tag);
        if (lane >= AUX_RETURNS) v = v != 0;
    }
    else if (L->auto_ch >= 0 && L->auto_ch < CONSOLE_CHANNELS)
    {
        lane = channel_lane(L->auto_tag);
        if (lane == AUTO_LANE_SW(0) && v > 2) v = 2;
        else if (lane > AUTO_LANE_SW(0)) v = v != 0;
    }
    if (lane < 0) return; // not a control this console has
    if (!L->store_ok || L->auto_t < L->last_t) { L->report->moves_lost++; return; }
    const store_ev_t e = {L->auto_t, (uint8_t)L->auto_ch, (uint8_t)lane, (uint16_t)v};
    if (!store_new_put(&e)) { L->store_ok = false; L->report->moves_lost++; return; }
    L->last_t = L->auto_t;
    L->used[L->auto_ch] |= 1ULL << lane;
    L->report->moves++;
}

static void leaf (loader_t *L, uint8_t depth, leaf_t type, const char *text, int32_t number)
{
    pending_t *p = &L->p;
    if (strcmp(L->path[0], "automation") == 0)
    {
        // [ms, channel, "tag", value]: its items are at depth 3
        if (depth != 3U) return;
        switch (L->elem[2])
        {
            case 0: L->auto_t = number < 0 ? 0U : (uint32_t)number; L->auto_ch = -1; L->auto_tag[0] = '\0'; break;
            case 1:
                if (type == LEAF_NUMBER && number >= 1 && number <= CONSOLE_CHANNELS) L->auto_ch = (int16_t)(number - 1);
                else if (type == LEAF_STRING && strcmp(text, "master") == 0) L->auto_ch = AUTO_MASTER_CH;
                break;
            case 2: if (type == LEAF_STRING) { strncpy(L->auto_tag, text, KEY_MAX); L->auto_tag[KEY_MAX] = '\0'; } break;
            case 3: if (type == LEAF_NUMBER || type == LEAF_BOOL) take_move(L, number); break;
            default: break;
        }
        return;
    }
    if (strcmp(L->path[0], "master") == 0)
    {
        // "master": { "knobs": { "aux_return_1": 49152 }, "toggles": { "aux_return_1_to_mix": true } }
        const int8_t lane = (depth == 3U) ? master_lane(L->path[2]) : -1;
        if (lane < 0 || (type != LEAF_NUMBER && type != LEAF_BOOL)) return;
        L->master[lane] = (uint16_t)(lane >= AUX_RETURNS ? (number != 0) : (number < 0 ? 0 : number > 65535 ? 65535 : number));
        L->have_master = true;
        return;
    }
    if (p->channel < 0 || strcmp(L->path[0], "channels") != 0) return;

    if (depth == 3U && strcmp(L->path[2], "name") == 0 && type == LEAF_STRING)
    {
        strncpy(p->name, text, CHANNEL_NAME_MAX);
        p->name[CHANNEL_NAME_MAX] = '\0';
    }
    else if (depth == 4U && (type == LEAF_NUMBER || type == LEAF_BOOL))
    {
        const char *group = L->path[2], *tag = L->path[3];
        if (strcmp(group, "knobs") == 0 && type == LEAF_NUMBER)
        {
            const int8_t id = knob_id(tag);
            if (id >= 0) p->v.knob[id] = (uint16_t)(number < 0 ? 0 : number > 65535 ? 65535 : number);
        }
        else if (strcmp(group, "toggles") == 0)
        {
            const int8_t i = toggle_index(tag);
            if (i >= 0)
            {
                if (TOGGLES[i].sw == TOGGLE_SEL) p->v.sel = number != 0;
                else p->v.sw[TOGGLES[i].sw] = number != 0 ? 1U : 0U;
            }
        }
        else if (strcmp(group, "cycles") == 0 && type == LEAF_NUMBER && strcmp(tag, "input_src") == 0)
        {
            p->v.sw[0] = (uint8_t)(number < 0 ? 0 : number > 2 ? 2 : number);
        }
    }
}

/** Sends one command to a bucket, waiting for room in its queue */
static bool send_cmd (uint8_t bucket, const char *fmt, ...)
{
    char cmd[40];
    va_list ap;
    va_start(ap, fmt);
    const int len = vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    return len > 0 && bucket_link_send_wait(bucket, cmd, (uint16_t)len, 1000U);
}

/** The channel object just finished: into the mix, and to its bucket if it's connected */
static void apply_channel (loader_t *L)
{
    const pending_t *p = &L->p;
    const uint16_t ch = (uint16_t)p->channel;
    if (p->channel < 0 || ch >= CONSOLE_CHANNELS) return;
    L->report->channels++;

    for (uint8_t id = 0; id < MIX_KNOBS; id++) mix_state_set_knob(ch, id, p->v.knob[id]);
    for (uint8_t id = 0; id < MIX_SWITCHES; id++) mix_state_set_switch(ch, id, p->v.sw[id]);

    // a name that is just the app's default ("Channel 3" on channel 3) is no name
    char label[16];
    snprintf(label, sizeof(label), "Channel %u", (unsigned)(ch + 1U));
    bucket_link_name(ch, strcmp(p->name, label) == 0 ? "" : p->name); // kept, and sent when the bucket is there

    const uint8_t bucket = (uint8_t)(ch / CHANNELS_PER_BUCKET);
    const unsigned local = ch % CHANNELS_PER_BUCKET;
    if (!bucket_link_listening(bucket))
    {
        L->report->skipped++;
        return;
    }
    // the input type first: the bucket only takes +48V on the mic input
    bool ok = send_cmd(bucket, "%%sw %u 0 %u%u%%", local, (unsigned)(p->v.sw[0] >> 1), (unsigned)(p->v.sw[0] & 1U));
    for (uint8_t id = 1; id < MIX_SWITCHES && ok; id++) ok = send_cmd(bucket, "%%sw %u %u %u%%", local, (unsigned)id, (unsigned)p->v.sw[id]);
    for (uint8_t id = 0; id < MIX_KNOBS && ok; id++) ok = send_cmd(bucket, "%%ud %u %u %u%%", local, (unsigned)id, (unsigned)p->v.knob[id]);
    if (ok) L->report->sent++;
    else L->report->skipped++; // it went away mid-load
}

static bool parse_value (loader_t *L, uint8_t depth);

static bool parse_object (loader_t *L, uint8_t depth)
{
    reader_t *r = &L->r;
    if (depth == 1U && strcmp(L->path[0], "automation") == 0) L->report->old_layout = true; // version 3's: not read
    r_ws(r);
    if (r_peek(r) == '}') { r->pos++; return true; }
    for (;;)
    {
        r_ws(r);
        if (r_next(r) != '"') return false;
        char key[KEY_MAX + 1];
        if (!r_string(r, key, sizeof(key))) return false;
        r_ws(r);
        if (r_next(r) != ':') return false;
        if (depth < DEPTH_MAX) memcpy(L->path[depth], key, sizeof(key));

        // a channel object starts: "Channel N" inside "channels"
        const bool channel_obj = depth == 1U && strcmp(L->path[0], "channels") == 0;
        if (channel_obj)
        {
            unsigned n = 0;
            L->p.channel = (sscanf(key, "Channel %u", &n) == 1 && n >= 1U) ? (int16_t)(n - 1U) : -1;
            for (uint8_t id = 0; id < MIX_KNOBS; id++) L->p.v.knob[id] = mix_state_default_knob(id);
            for (uint8_t id = 0; id < MIX_SWITCHES; id++) L->p.v.sw[id] = mix_state_default_switch(id);
            L->p.v.sel = false;
            L->p.name[0] = '\0';
        }
        if (!parse_value(L, (uint8_t)(depth + 1U))) return false;
        if (channel_obj)
        {
            apply_channel(L);
            L->p.channel = -1;
        }

        r_ws(r);
        const int c = r_next(r);
        if (c == '}') return true;
        if (c != ',') return false;
    }
}

static bool parse_array (loader_t *L, uint8_t depth)
{
    reader_t *r = &L->r;
    r_ws(r);
    if (r_peek(r) == ']') { r->pos++; return true; }
    for (uint16_t i = 0;; i++)
    {
        if (depth <= DEPTH_MAX) L->elem[depth] = i;
        if (!parse_value(L, (uint8_t)(depth + 1U))) return false;
        r_ws(r);
        const int c = r_next(r);
        if (c == ']') return true;
        if (c != ',') return false;
    }
}

static bool literal (reader_t *r, const char *rest)
{
    for (; *rest; rest++) if (r_next(r) != *rest) return false;
    return true;
}

static bool parse_value (loader_t *L, uint8_t depth)
{
    reader_t *r = &L->r;
    if (depth > DEPTH_MAX) return false;
    r_ws(r);
    const int c = r_next(r);
    switch (c)
    {
        case '{': return parse_object(L, depth);
        case '[': return parse_array(L, depth);
        case '"':
        {
            char text[KEY_MAX + 8];
            if (!r_string(r, text, sizeof(text))) return false;
            leaf(L, depth, LEAF_STRING, text, 0);
            return true;
        }
        case 't': if (!literal(r, "rue")) return false; leaf(L, depth, LEAF_BOOL, NULL, 1); return true;
        case 'f': if (!literal(r, "alse")) return false; leaf(L, depth, LEAF_BOOL, NULL, 0); return true;
        case 'n': if (!literal(r, "ull")) return false; leaf(L, depth, LEAF_NULL, NULL, 0); return true;
        default: break;
    }
    if (c == '-' || (c >= '0' && c <= '9'))
    {
        // a number: the whole part (the app writes whole numbers); fraction and exponent read past
        const bool negative = c == '-';
        int32_t v = negative ? 0 : c - '0';
        int n;
        while ((n = r_peek(r)) >= '0' && n <= '9') { r->pos++; if (v < 100000000) v = v * 10 + (n - '0'); }
        while ((n = r_peek(r)) == '.' || n == 'e' || n == 'E' || n == '+' || n == '-' || (n >= '0' && n <= '9')) r->pos++;
        leaf(L, depth, LEAF_NUMBER, NULL, negative ? -v : v);
        return true;
    }
    return false;
}

static mix_result_t do_load (const char *name, mix_load_report_t *report)
{
    static mix_load_report_t dummy;
    if (report == NULL) report = &dummy;
    *report = (mix_load_report_t){0};
    if (automation_busy()) return MIX_BUSY;
    const mix_result_t m = do_mount();
    if (m != MIX_OK) return m;

    char path[MIX_NAME_MAX + 8];
    path_of(path, sizeof(path), name);
    FRESULT fr = f_open(&USERFile, path, FA_READ);
    if (fr != FR_OK)
    {
        card_failed(fr);
        return (fr == FR_NO_FILE) ? MIX_NOT_FOUND : MIX_DISK_ERROR;
    }

    static loader_t L; // kept off the task's stack
    memset(&L, 0, sizeof(L));
    L.r.f = &USERFile;
    L.p.channel = -1;
    L.auto_ch = -1;
    L.report = report;
    for (uint8_t i = 0; i < MIX_MASTER; i++) L.master[i] = mix_state_get_master(i); // anything not in the file stays
    L.store_ok = store_new_open(); // its moves go straight to the card

    const bool parsed = parse_value(&L, 0);
    const bool read_error = L.r.err;
    (void)f_close(&USERFile);

    if (L.have_master)
    {
        uint16_t level[AUX_RETURNS];
        bool to_mix[AUX_RETURNS];
        for (uint8_t i = 0; i < AUX_RETURNS; i++)
        {
            mix_state_set_master(i, L.master[i]);
            mix_state_set_master((uint8_t)(AUX_RETURNS + i), L.master[AUX_RETURNS + i]);
            level[i] = L.master[i];
            to_mix[i] = L.master[AUX_RETURNS + i] != 0U;
        }
        control_set_aux_returns(level, to_mix);
    }
    // the file's moves replace the mix's (none, for a version 2 or 3 file)
    const bool keep = parsed && !read_error && L.store_ok;
    if (store_new_close(keep) && keep) automation_store_loaded(L.used);

    if (read_error)
    {
        card_failed(FR_DISK_ERR);
        return MIX_DISK_ERROR;
    }
    if (!parsed || report->channels == 0U) return MIX_BAD_FILE;

    // its autosaves go next to it: "<name> (autosave).dgl"
    snprintf(s_auto_base, sizeof(s_auto_base), "%s", name);
    char *dot = strrchr(s_auto_base, '.');
    if (dot != NULL) *dot = '\0';
    const size_t n = strlen(s_auto_base), suffix = strlen(AUTOSAVE_SUFFIX);
    if (n >= suffix && strcmp(s_auto_base + n - suffix, AUTOSAVE_SUFFIX) == 0) s_auto_base[n - suffix] = '\0';
    mark_saved();
    return MIX_OK;
}


/*=============================== THE STORAGE TASK ================================*/

typedef enum { JOB_NONE = 0, JOB_MOUNT, JOB_LIST, JOB_SAVE, JOB_LOAD, JOB_DELETE, JOB_FORMAT } job_op_t;

static struct
{
    volatile job_op_t op;
    const char *name;
    char (*names)[MIX_NAME_MAX + 1];
    uint16_t max, count;
    char *saved;
    mix_load_report_t *report;
    mix_result_t result;
} s_job;

static TaskHandle_t s_task;
static SemaphoreHandle_t s_job_lock, s_job_done;
static StaticSemaphore_t s_job_lock_mem, s_job_done_mem;
static uint32_t s_stack[768]; // 3 KB: FatFs, the JSON reader and writer
static StaticTask_t s_tcb;

static void do_job (void)
{
    switch (s_job.op)
    {
        case JOB_MOUNT:  s_job.result = do_mount(); break;
        case JOB_LIST:   s_job.count = do_list(s_job.names, s_job.max); s_job.result = MIX_OK; break;
        case JOB_SAVE:   s_job.result = do_save_new(s_job.saved); break;
        case JOB_LOAD:   s_job.result = do_load(s_job.name, s_job.report); break;
        case JOB_DELETE: s_job.result = do_delete(s_job.name); break;
        case JOB_FORMAT: s_job.result = do_format(); break;
        default: break;
    }
    s_job.op = JOB_NONE;
}

/** Hands the job in s_job to the storage task and waits for it (or does it here, before the tasks run) */
static void run_job (void)
{
    if (s_task == NULL || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || xTaskGetCurrentTaskHandle() == s_task)
    {
        do_job();
        return;
    }
    xTaskNotifyGive(s_task);
    (void)xSemaphoreTake(s_job_done, portMAX_DELAY);
}

static void job_begin (job_op_t op)
{
    if (s_job_lock != NULL) (void)xSemaphoreTake(s_job_lock, portMAX_DELAY); // one at a time
    s_job.op = op;
}

static void job_end (void)
{
    if (s_job_lock != NULL) (void)xSemaphoreGive(s_job_lock);
}

mix_result_t mix_file_mount (void)
{
    job_begin(JOB_MOUNT);
    run_job();
    const mix_result_t r = s_job.result;
    job_end();
    return r;
}

uint16_t mix_file_list (char names[][MIX_NAME_MAX + 1], uint16_t max)
{
    job_begin(JOB_LIST);
    s_job.names = names;
    s_job.max = max;
    run_job();
    const uint16_t n = s_job.count;
    job_end();
    return n;
}

mix_result_t mix_file_save_new (char saved_name[MIX_NAME_MAX + 1])
{
    job_begin(JOB_SAVE);
    s_job.saved = saved_name;
    run_job();
    const mix_result_t r = s_job.result;
    job_end();
    return r;
}

mix_result_t mix_file_load (const char *name, mix_load_report_t *report)
{
    job_begin(JOB_LOAD);
    s_job.name = name;
    s_job.report = report;
    run_job();
    const mix_result_t r = s_job.result;
    job_end();
    return r;
}

mix_result_t mix_file_delete (const char *name)
{
    job_begin(JOB_DELETE);
    s_job.name = name;
    run_job();
    const mix_result_t r = s_job.result;
    job_end();
    return r;
}

mix_result_t mix_file_format (void)
{
    job_begin(JOB_FORMAT);
    run_job();
    const mix_result_t r = s_job.result;
    job_end();
    return r;
}

void mix_file_wake (void)
{
    if (s_task != NULL) xTaskNotifyGive(s_task);
}

void mix_file_service (void)
{
    // the card: mounted at start, and tried again every couple of seconds while it isn't there
    if (!s_mounted && (!s_tried_mount || (TickType_t)(xTaskGetTickCount() - s_last_mount_try) >= pdMS_TO_TICKS(MOUNT_RETRY_MS)))
    {
        (void)do_mount();
    }
    if (s_job.op != JOB_NONE) // the screen's
    {
        do_job();
        (void)xSemaphoreGive(s_job_done);
    }
    if (s_mounted) automation_service(); // moves to and from the card
    autosave_check();
}

static void storage_task (void *argument)
{
    (void)argument;
    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)); // woken for work, or every 100 ms
        mix_file_service();
    }
}

void mix_file_start (void)
{
    s_job_lock = xSemaphoreCreateMutexStatic(&s_job_lock_mem);
    s_job_done = xSemaphoreCreateBinaryStatic(&s_job_done_mem);
    mark_saved();
    static const osThreadAttr_t attr = {
        .name = "mix_storage",
        // below the bucket link (which never waits on it), above the display (which waits on it)
        .priority = osPriorityLow4,
        .stack_mem = s_stack,
        .stack_size = sizeof(s_stack),
        .cb_mem = &s_tcb,
        .cb_size = sizeof(s_tcb),
    };
    s_task = (TaskHandle_t)osThreadNew(storage_task, NULL, &attr);
}
