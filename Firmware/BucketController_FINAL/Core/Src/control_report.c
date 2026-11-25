/**
  **********************************************************************************
  * CONTROL REPORTS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file control_report.c
  * @brief See control_report.h.
  **********************************************************************************
*/

#include "control_report.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*=============================== IDS ================================*/

/* ud ids, numbered as in the remote app's CONFIG.yaml. Positions travel as 0-65535 across the knob's
   whole range, so a signed (centred) control's stored value is shifted by 32768. */
static const struct
{
    uint16_t offset;
    bool is_signed;
} KNOBS[REPORT_KNOBS] = {
    {offsetof(channel_controls, input_gain), false},                //  0 input gain
    {offsetof(channel_controls, send_controls[0].gain), false},     //  1 send 1
    {offsetof(channel_controls, send_controls[0].pan), true},       //  2 send 1 pan
    {offsetof(channel_controls, send_controls[1].gain), false},     //  3 send 2
    {offsetof(channel_controls, send_controls[1].pan), true},       //  4 send 2 pan
    {offsetof(channel_controls, send_controls[2].gain), false},     //  5 send 3
    {offsetof(channel_controls, send_controls[2].pan), true},       //  6 send 3 pan
    {offsetof(channel_controls, send_controls[3].gain), false},     //  7 send 4
    {offsetof(channel_controls, hf_control.gain), true},            //  8 HF gain
    {offsetof(channel_controls, hf_control.freq), false},           //  9 HF freq
    {offsetof(channel_controls, hmf_control.gain), true},           // 10 HMF gain
    {offsetof(channel_controls, hmf_control.freq), false},          // 11 HMF freq
    {offsetof(channel_controls, hmf_control.q), false},             // 12 HMF Q
    {offsetof(channel_controls, lmf_control.gain), true},           // 13 LMF gain
    {offsetof(channel_controls, lmf_control.freq), false},          // 14 LMF freq
    {offsetof(channel_controls, lmf_control.q), false},             // 15 LMF Q
    {offsetof(channel_controls, lf_control.gain), true},            // 16 LF gain
    {offsetof(channel_controls, lf_control.freq), false},           // 17 LF freq
    {offsetof(channel_controls, comp_control.in_gain), false},      // 18 comp input
    {offsetof(channel_controls, comp_control.attack_time), false},  // 19 attack
    {offsetof(channel_controls, comp_control.threshold), false},    // 20 threshold
    {offsetof(channel_controls, comp_control.release_time), false}, // 21 release
    {offsetof(channel_controls, comp_control.de_ess_amt), false},   // 22 de-ess (not used, kept for the numbering)
    {offsetof(channel_controls, comp_control.ratio), false},        // 23 ratio
    {offsetof(channel_controls, comp_control.out_gain), false},     // 24 comp output
    {offsetof(channel_controls, output_pan), true},                 // 25 pan
    {offsetof(channel_controls, output_gain), true},                // 26 fader
};

bool report_knob_value (const channel_controls *v, uint8_t id, uint16_t *out)
{
    if (id >= REPORT_KNOBS || v == NULL || out == NULL) return false;
    const uint8_t *field = (const uint8_t *)v + KNOBS[id].offset;
    if (KNOBS[id].is_signed)
    {
        s_scalar_control_t s;
        memcpy(&s, field, sizeof(s));
        *out = (uint16_t)((int32_t)s + 32768);
    }
    else
    {
        u_scalar_control_t u;
        memcpy(&u, field, sizeof(u));
        *out = u;
    }
    return true;
}

bool report_set_knob (channel_controls *v, uint8_t id, uint16_t value)
{
    if (id >= REPORT_KNOBS || v == NULL) return false;
    uint8_t *field = (uint8_t *)v + KNOBS[id].offset;
    if (KNOBS[id].is_signed)
    {
        const s_scalar_control_t s = (s_scalar_control_t)((int32_t)value - 32768); // centre = 0
        memcpy(field, &s, sizeof(s));
    }
    else
    {
        const u_scalar_control_t u = value;
        memcpy(field, &u, sizeof(u));
    }
    return true;
}

uint8_t report_switch_value (const channel_controls *v, uint8_t id)
{
    switch (id)
    {
        case 0: // the app's order: line, mic, hi-z
            return v->input_type_selection == MIC_INPUT ? 1U : (v->input_type_selection == HIZ_INPUT ? 2U : 0U);
        case 1: return v->phase_flip ? 1U : 0U;
        case 2: return v->phantom_48v ? 1U : 0U;
        case 3: return v->high_pass_filter ? 1U : 0U;
        case 4: return v->solo ? 1U : 0U;
        case 5: return v->muted ? 1U : 0U;
        case 6: return v->ins ? 1U : 0U;
        case 7: return v->pre_fader ? 1U : 0U;
        default: return 0U;
    }
}

/*=============================== SNAPSHOT ================================*/

#define ITEMS_PER_CHANNEL (REPORT_KNOBS + REPORT_SWITCHES)
#define ITEM_COUNT        (CHANNELS * ITEMS_PER_CHANNEL + 1) // + the selection, last
#define ITEM_SELECT       (CHANNELS * ITEMS_PER_CHANNEL)

_Static_assert(ITEMS_PER_CHANNEL <= 64, "one 64-bit mask of waiting reports per channel");

static uint16_t s_knob[CHANNELS][REPORT_KNOBS];      // as last reported (or set by the master)
static uint8_t s_switch[CHANNELS][REPORT_SWITCHES];
static int8_t s_selected = -1;
static uint64_t s_waiting[CHANNELS];                 // bit per item: knobs 0-26, then switches
static bool s_select_waiting;
static bool s_seen[CHANNELS];                        // the first look at a channel reports everything
static bool s_select_seen;
static uint16_t s_next;                              // the item the next report_build starts from

void report_scan (uint8_t channel, const channel_controls *now)
{
    if (channel >= CHANNELS || now == NULL) return;
    for (uint8_t id = 0; id < REPORT_KNOBS; id++)
    {
        uint16_t value;
        report_knob_value(now, id, &value);
        if (!s_seen[channel] || value != s_knob[channel][id])
        {
            s_knob[channel][id] = value;
            s_waiting[channel] |= (uint64_t)1 << id;
        }
    }
    for (uint8_t id = 0; id < REPORT_SWITCHES; id++)
    {
        const uint8_t value = report_switch_value(now, id);
        if (!s_seen[channel] || value != s_switch[channel][id])
        {
            s_switch[channel][id] = value;
            s_waiting[channel] |= (uint64_t)1 << (REPORT_KNOBS + id);
        }
    }
    s_seen[channel] = true;
}

void report_scan_select (int8_t selected)
{
    if (!s_select_seen || selected != s_selected)
    {
        s_selected = selected;
        s_select_waiting = true;
    }
    s_select_seen = true;
}

void report_remote_knob (uint8_t channel, uint8_t id, uint16_t value)
{
    if (channel >= CHANNELS || id >= REPORT_KNOBS) return;
    s_knob[channel][id] = value;
    s_waiting[channel] &= ~((uint64_t)1 << id); // the master already has it
}

void report_remote_switch (uint8_t channel, uint8_t id, uint8_t value)
{
    if (channel >= CHANNELS || id >= REPORT_SWITCHES) return;
    s_switch[channel][id] = value;
    s_waiting[channel] &= ~((uint64_t)1 << (REPORT_KNOBS + id));
}

void report_remote_select (int8_t selected)
{
    s_selected = selected;
    s_select_waiting = false;
    s_select_seen = true;
}

void report_mark_switch (uint8_t channel, uint8_t id)
{
    if (channel >= CHANNELS || id >= REPORT_SWITCHES) return;
    s_waiting[channel] |= (uint64_t)1 << (REPORT_KNOBS + id);
}

void report_dump (void)
{
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        // a channel not looked at yet is reported whole on its first look anyway
        s_waiting[ch] = ((uint64_t)1 << ITEMS_PER_CHANNEL) - 1U;
    }
    s_select_waiting = true;
}

bool report_pending (void)
{
    if (s_select_waiting) return true;
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        if (s_waiting[ch] != 0U) return true;
    }
    return false;
}

/*=============================== BUILDING ================================*/

static bool item_waiting (uint16_t item)
{
    if (item == ITEM_SELECT) return s_select_waiting;
    return (s_waiting[item / ITEMS_PER_CHANNEL] >> (item % ITEMS_PER_CHANNEL)) & 1U;
}

static void item_done (uint16_t item)
{
    if (item == ITEM_SELECT) s_select_waiting = false;
    else s_waiting[item / ITEMS_PER_CHANNEL] &= ~((uint64_t)1 << (item % ITEMS_PER_CHANNEL));
}

/** One report's text, e.g. "ud 5 25 40000", "sw 4 0 01", "sel 6", "sel -" */
static int item_text (uint16_t item, int16_t first_index, char *text, size_t size)
{
    if (item == ITEM_SELECT)
    {
        if (s_selected < 0) return snprintf(text, size, "sel -");
        return snprintf(text, size, "sel %d", (int)(first_index + s_selected));
    }
    const uint8_t local = (uint8_t)(item / ITEMS_PER_CHANNEL);
    const uint8_t which = (uint8_t)(item % ITEMS_PER_CHANNEL);
    const int ch = (int)(first_index + local);
    if (which < REPORT_KNOBS)
    {
        return snprintf(text, size, "ud %d %u %u", ch, (unsigned)which, (unsigned)s_knob[local][which]);
    }
    const uint8_t id = (uint8_t)(which - REPORT_KNOBS);
    const uint8_t value = s_switch[local][id];
    if (id == 0) return snprintf(text, size, "sw %d 0 %u%u", ch, (unsigned)(value >> 1), (unsigned)(value & 1U)); // "00" "01" "10"
    return snprintf(text, size, "sw %d %u %u", ch, (unsigned)id, (unsigned)value);
}

uint16_t report_build (char *out, uint16_t room, int16_t first_index)
{
    if (out == NULL || room == 0U) return 0;
    out[0] = '\0';
    if (first_index < 0) return 0; // console channels not known yet: keep everything waiting

    uint16_t len = 0;
    uint16_t item = s_next % ITEM_COUNT;
    for (uint16_t n = 0; n < ITEM_COUNT; n++, item = (uint16_t)((item + 1U) % ITEM_COUNT))
    {
        if (!item_waiting(item)) continue;
        char text[24];
        const int t = item_text(item, first_index, text, sizeof(text));
        if (t <= 0 || t >= (int)sizeof(text)) continue;
        const uint16_t need = (uint16_t)(t + (len ? 1 : 0)); // ';' between reports
        if (len + need + 1U > room) // keep room for the '\0'
        {
            s_next = item; // this one goes first next time
            return len;
        }
        if (len) out[len++] = ';';
        memcpy(&out[len], text, (size_t)t);
        len = (uint16_t)(len + t);
        out[len] = '\0';
        item_done(item);
    }
    s_next = item;
    return len;
}
