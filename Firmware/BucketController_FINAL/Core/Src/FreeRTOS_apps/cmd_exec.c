/**
  **********************************************************************************
  * COMMANDS FROM THE MASTER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/cmd_exec.c
  * @brief See cmd_exec.h.
  **********************************************************************************
*/

#include "FreeRTOS_apps/cmd_exec.h"
#include "FreeRTOS_apps/master_link.h"
#include "control_integration/control_interface.h"
#include "control_report.h"
#include "console_state_bucket.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

static uint32_t s_done, s_rejected;

/*=============================== PARSING ================================*/

static const char *skip_spaces (const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/** A whole number from *p (after any spaces), at most max; *p moves past it */
static bool read_number (const char **p, uint32_t max, uint32_t *out)
{
    const char *s = skip_spaces(*p);
    if (*s < '0' || *s > '9') return false;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9')
    {
        v = v * 10U + (uint32_t)(*s - '0');
        if (v > max) return false;
        s++;
    }
    *p = s;
    *out = v;
    return true;
}

/** True if p starts with the word w followed by a space or the end */
static bool is_verb (const char *p, const char *w)
{
    const size_t n = strlen(w);
    return strncmp(p, w, n) == 0 && (p[n] == '\0' || p[n] == ' ' || p[n] == '\t');
}

/*=============================== COMMANDS ================================*/

static bool do_ud (const char *p)
{
    uint32_t ch, id, value;
    if (!read_number(&p, CHANNELS - 1U, &ch) || !read_number(&p, REPORT_KNOBS - 1U, &id) ||
        !read_number(&p, 65535U, &value))
    {
        return false;
    }
    // the fader: not while someone has a hand on it, or the motor would fight them
    if (id == KNOB_FADER && fader_control_touched((uint8_t)ch)) return true;

    xSemaphoreTake(channel_control_mutexes[ch], portMAX_DELAY);
    report_set_knob(&channel_control_vals[ch], (uint8_t)id, (uint16_t)value);
    xSemaphoreGive(channel_control_mutexes[ch]);
    report_remote_knob((uint8_t)ch, (uint8_t)id, (uint16_t)value);
    return true;
}

static bool do_sw (const char *p)
{
    uint32_t ch, id;
    if (!read_number(&p, CHANNELS - 1U, &ch) || !read_number(&p, REPORT_SWITCHES - 1U, &id)) return false;
    p = skip_spaces(p);

    uint8_t value;
    if (id == 0U) // input type: "00" line, "01" mic, "10" hi-z (a single 0/1/2 digit also taken)
    {
        if (p[0] >= '0' && p[0] <= '1' && p[1] >= '0' && p[1] <= '1') value = (uint8_t)((p[0] - '0') * 2 + (p[1] - '0'));
        else if (p[0] >= '0' && p[0] <= '2' && (p[1] == '\0' || p[1] == ' ')) value = (uint8_t)(p[0] - '0');
        else return false;
        if (value > 2U) return false;
    }
    else
    {
        if ((p[0] != '0' && p[0] != '1') || (p[1] != '\0' && p[1] != ' ')) return false;
        value = (uint8_t)(p[0] - '0');
    }

    channel_controls *v = &channel_control_vals[ch];
    bool refused = false;
    xSemaphoreTake(channel_control_mutexes[ch], portMAX_DELAY);
    switch (id)
    {
        case 0:
            v->input_type_selection = value == 1U ? MIC_INPUT : (value == 2U ? HIZ_INPUT : LINE_INPUT);
            if (v->input_type_selection != MIC_INPUT) v->phantom_48v = false; // +48 V only on the mic input
            break;
        case 1: v->phase_flip = value != 0U; break;
        case 2:
            if (value != 0U && v->input_type_selection != MIC_INPUT) refused = true; // as the panel: mic only
            else v->phantom_48v = value != 0U;
            break;
        case 3: v->high_pass_filter = value != 0U; break;
        case 4: v->solo = value != 0U; break;
        case 5: v->muted = value != 0U; break;
        case 6: v->ins = value != 0U; break;
        case 7: v->pre_fader = value != 0U; break;
        default: break;
    }
    xSemaphoreGive(channel_control_mutexes[ch]);

    if (refused) report_mark_switch((uint8_t)ch, 2); // tell the master it stayed off
    else report_remote_switch((uint8_t)ch, (uint8_t)id, value);
    // (switching away from mic can turn +48 V off: that change is reported, so the app shows it)
    return true;
}

static bool do_sel (const char *p)
{
    p = skip_spaces(p);
    bool chosen[CHANNELS] = {false};
    int8_t first = -1;
    if (*p == '-')
    {
        // none of this bucket's channels
    }
    else
    {
        for (;;)
        {
            uint32_t ch;
            if (!read_number(&p, CHANNELS - 1U, &ch)) return false;
            chosen[ch] = true;
            if (first < 0) first = (int8_t)ch;
            if (*p != ':') break;
            p++;
        }
    }
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        xSemaphoreTake(channel_control_mutexes[ch], portMAX_DELAY);
        channel_control_vals[ch].sel = chosen[ch];
        xSemaphoreGive(channel_control_mutexes[ch]);
    }
    report_remote_select(first);
    return true;
}

static bool do_chnm (const char *p)
{
    uint32_t ch;
    if (!read_number(&p, CHANNELS - 1U, &ch)) return false;
    console_state_set_name((uint8_t)ch, p); // the rest of the line (spaces trimmed there)
    return true;
}

bool cmd_exec (const char *frame)
{
    const char *p = skip_spaces(frame);
    bool ok;
    if (is_verb(p, "ud")) ok = do_ud(p + 2);
    else if (is_verb(p, "sw")) ok = do_sw(p + 2);
    else if (is_verb(p, "ack"))
    {
        uint32_t seq;
        const char *q = p + 3;
        ok = read_number(&q, 255U, &seq);
        if (ok) master_link_ack((uint8_t)seq);
    }
    else if (is_verb(p, "base"))
    {
        uint32_t first;
        const char *q = p + 4;
        ok = read_number(&q, 255U, &first) && console_state_set_base(first);
    }
    else if (is_verb(p, "sel")) ok = do_sel(p + 3);
    else if (is_verb(p, "chnm")) ok = do_chnm(p + 4);
    else if (is_verb(p, "dump"))
    {
        report_dump();
        ok = true;
    }
    else if (is_verb(p, "sett") || is_verb(p, "md")) ok = true; // nothing to do yet
    else return false;                                         // not a command: out of step

    if (ok) s_done++;
    else s_rejected++;
    return true;
}

void cmd_exec_counts (uint32_t *done, uint32_t *rejected)
{
    if (done) *done = s_done;
    if (rejected) *rejected = s_rejected;
}
