/**
  **********************************************************************************
  * CHANNEL DISPLAYS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/channel_display.c
  * @brief See channel_display.h. What each screen shows is laid out in ui/channel_screen.c;
  *        displays.c sends it to the panels.
  *
  * Nothing is redrawn unless it changed:
  *  - the whole screen when mute or solo changes (the background colour changes)
  *  - the title strip when the input type changes, or as a selected channel's strip flashes
  *  - the EQ graph's area when an EQ setting changes, or when the knobs switch between
  *    EQ, sends and compressor (the graph only shows for EQ)
  *  - the pan bar when the pan moves by at least a pixel
  **********************************************************************************
*/

#include "FreeRTOS_apps/channel_display.h"
#include "FreeRTOS_apps/displays.h"
#include "ui/channel_screen.h"
#include "control_integration/control_interface.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cv_mapping.h"

#define CHANNEL_DISPLAY_PERIOD_MS 33U   // about 30 checks a second
#define SELECT_FLASH_MS           500U  // selected channel: title strip lighter for this long, then normal for this long
#define POPUP_MS                  2000U // a changed value stays on screen this long after the knob stops

_Static_assert(CHANNELS <= DISPLAY_COUNT, "one screen per channel");

/** The channel each screen shows (screen 0 shows channel 0, labelled 01). Reorder if the screens are mounted differently. */
static const uint8_t DISPLAY_CHANNEL[CHANNELS] = {0, 1, 2, 3};

/** What is on each screen now, so only changes get redrawn */
typedef struct
{
    bool drawn;                   //!< false until the screen has been drawn once
    channel_screen_state_t state;
    channel_eq_t eq;              //!< the settings the graph on screen was drawn from (if state.show_eq)
    int16_t fader;                //!< what the sends window was drawn from: master level...
    uint16_t send_level[4];       //!< ...the four sends...
    int16_t send_pan[2];          //!< ...and the pans of sends 1 and 2
    char popup[16];               //!< the pop-up text on screen (state.popup_text points here)
} shown_t;

static shown_t s_shown[CHANNELS];
static channel_views_t s_views[CHANNELS]; // each screen's worked-out EQ graph and sends window (only this task draws)

static channel_eq_band_t eq_band (const eq_band_control_t *band)
{
    return (channel_eq_band_t){.gain = band->gain, .q = band->q, .freq = band->freq, .bell = band->bell};
}

static bool same_band (const channel_eq_band_t *a, const channel_eq_band_t *b)
{
    return a->gain == b->gain && a->q == b->q && a->freq == b->freq && a->bell == b->bell;
}

static bool same_eq (const channel_eq_t *a, const channel_eq_t *b)
{
    return a->hpf == b->hpf && same_band(&a->hf, &b->hf) && same_band(&a->hmf, &b->hmf) &&
           same_band(&a->lmf, &b->lmf) && same_band(&a->lf, &b->lf);
}

static channel_input_t input_type (channel_input_selection_t selection)
{
    switch (selection)
    {
        case MIC_INPUT: return CH_INPUT_MIC;
        case HIZ_INPUT: return CH_INPUT_HIZ;
        default:        return CH_INPUT_LINE;
    }
}

/*=============================== VALUE POP-UP TEXT ================================*/
/* Printed with whole numbers only: the C library here is built without %f support. */

/** dB to the nearest 0.05, e.g. "+3.45 dB", "-12.00 dB", "0.00 dB" */
static void text_db (char *out, size_t size, float db)
{
    long hundredths = lroundf(db * 20.0f) * 5L;
    if (hundredths == 0)
    {
        snprintf(out, size, "0.00 dB");
        return;
    }
    const char sign = hundredths < 0 ? '-' : '+';
    if (hundredths < 0) hundredths = -hundredths;
    snprintf(out, size, "%c%ld.%02ld dB", sign, hundredths / 100L, hundredths % 100L);
}

/** Frequency with three significant figures: "65.0 Hz", "189 Hz", "1.87 kHz", "12.4 kHz" */
static void text_hz (char *out, size_t size, float hz)
{
    if (hz < 99.95f)
    {
        const long tenths = lroundf(hz * 10.0f);
        snprintf(out, size, "%ld.%ld Hz", tenths / 10L, tenths % 10L);
    }
    else if (hz < 999.5f)
    {
        snprintf(out, size, "%ld Hz", lroundf(hz));
    }
    else if (hz < 9995.0f)
    {
        const long hundredths_khz = lroundf(hz / 10.0f);
        snprintf(out, size, "%ld.%02ld kHz", hundredths_khz / 100L, hundredths_khz % 100L);
    }
    else
    {
        const long tenths_khz = lroundf(hz / 100.0f);
        snprintf(out, size, "%ld.%ld kHz", tenths_khz / 10L, tenths_khz % 10L);
    }
}

/** Pan as "C", "50L", "33R": how far from the centre, in percent of the way to fully left or right */
static void text_pan (char *out, size_t size, int16_t pan)
{
    channel_screen_pan_text(out, size, pan); // the same text the sends window shows under A1 and A2
}

/** A send's level: "OFF" fully down, otherwise its dB, L = 10 + 60 log10(x) (cv_mapping.pdf 12-15) */
static void text_send (char *out, size_t size, uint16_t level)
{
    if (level == 0)
    {
        snprintf(out, size, "OFF");
        return;
    }
    text_db(out, size, 10.0f + 60.0f * log10f((float)level / 65535.0f));
}

/** The pop-up for the value last changed on this channel, if that was in the last POPUP_MS */
static channel_popup_where_t popup_for (uint8_t channel, const channel_controls *v, char *text, size_t size, uint32_t *rgb)
{
    const touched_t t = control_last_touched(channel);
    if (t.param == TOUCHED_NOTHING || (uint32_t)(xTaskGetTickCount() - t.tick) >= pdMS_TO_TICKS(POPUP_MS))
    {
        return CH_POPUP_NONE;
    }

    const eq_band_control_t *bands[4] = {&v->lf_control, &v->lmf_control, &v->hmf_control, &v->hf_control};
    const uint8_t band = t.band < 4 ? t.band : 0;
    const eq_band_control_t *b = bands[band];
    const bool shelf = (band == 0 || band == 3) && !b->bell; // LF and HF are shelves unless switched to bell

    switch (t.param)
    {
        case TOUCHED_INPUT_GAIN:
        {
            const cv_source_t source = v->input_type_selection == MIC_INPUT ? CV_SOURCE_MIC :
                                       v->input_type_selection == HIZ_INPUT ? CV_SOURCE_HIZ : CV_SOURCE_LINE;
            text_db(text, size, cv_input_gain_db(source, v->input_gain));
            *rgb = CH_COLOR_POPUP_INPUT;
            return CH_POPUP_TOP;
        }
        case TOUCHED_PAN:
            text_pan(text, size, v->output_pan);
            *rgb = CH_COLOR_POPUP_PAN;
            return CH_POPUP_LOW;
        case TOUCHED_EQ_GAIN:
            text_db(text, size, cv_eq_gain_db(cv_signed_to_p(b->gain), shelf));
            *rgb = CH_COLOR_POPUP_EQ;
            return CH_POPUP_TOP;
        case TOUCHED_EQ_FREQ:
            text_hz(text, size, channel_eq_freq_hz((channel_eq_band_index_t)band, b->freq));
            *rgb = CH_COLOR_POPUP_EQ;
            return CH_POPUP_TOP;
        case TOUCHED_EQ_Q:
        {
            const long hundredths = lroundf(channel_eq_q(b->q) * 100.0f);
            snprintf(text, size, "Q %ld.%02ld", hundredths / 100L, hundredths % 100L);
            *rgb = CH_COLOR_POPUP_EQ;
            return CH_POPUP_TOP;
        }
        case TOUCHED_SEND_LEVEL:
            text_send(text, size, v->send_controls[t.band < 4 ? t.band : 0].gain);
            *rgb = CH_COLOR_POPUP_SEND;
            return CH_POPUP_TOP;
        case TOUCHED_SEND_PAN:
            text_pan(text, size, v->send_controls[t.band < 2 ? t.band : 0].pan);
            *rgb = CH_COLOR_POPUP_SEND;
            return CH_POPUP_TOP;
        default:
            return CH_POPUP_NONE;
    }
}

/*=============================== DRAWING ================================*/

static void update_display (uint8_t display, bool flash_on)
{
    const uint8_t channel = DISPLAY_CHANNEL[display];
    shown_t *shown = &s_shown[display];

    // copy what this screen needs while holding the channel's lock, then draw without it
    xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
    const channel_controls v = channel_control_vals[channel];
    xSemaphoreGive(channel_control_mutexes[channel]);

    char popup[sizeof(shown->popup)] = "";
    uint32_t popup_rgb = 0;
    const channel_popup_where_t where = popup_for(channel, &v, popup, sizeof(popup), &popup_rgb);

    channel_screen_state_t now = {
        .number = (uint8_t)(channel + 1U),
        .input = input_type(v.input_type_selection),
        .muted = v.muted,
        .soloed = v.solo,
        .title_lit = v.sel && flash_on,
        .show_eq = (v.control_set_selection == EQ_CONTROL),
        .show_sends = (v.control_set_selection == SENDS_CONTROL),
        .pan_fill = channel_screen_pan_fill(v.output_pan),
        .popup = where,
        .popup_text = shown->popup, // filled in below once it's known what changed
        .popup_rgb = popup_rgb,
    };
    const channel_eq_t eq = {
        .hf = eq_band(&v.hf_control),
        .hmf = eq_band(&v.hmf_control),
        .lmf = eq_band(&v.lmf_control),
        .lf = eq_band(&v.lf_control),
        .hpf = v.high_pass_filter,
    };

    channel_screen_state_t *was = &shown->state;
    channel_views_t *views = &s_views[display];
    ui_region_t region;

    const bool eq_changed = !shown->drawn || !same_eq(&eq, &shown->eq);
    if (eq_changed) channel_screen_eq_compute(&views->eq, &eq); // kept current even while the graph is hidden
    shown->eq = eq;

    // sends window: the fader (master level), the four sends and the pans of sends 1 and 2
    const uint16_t send_level[4] = {v.send_controls[0].gain, v.send_controls[1].gain, v.send_controls[2].gain,
                                    v.send_controls[3].gain};
    const int16_t send_pan[2] = {v.send_controls[0].pan, v.send_controls[1].pan};
    const bool sends_changed = !shown->drawn || v.output_gain != shown->fader ||
                               memcmp(send_level, shown->send_level, sizeof(send_level)) != 0 ||
                               memcmp(send_pan, shown->send_pan, sizeof(send_pan)) != 0;
    if (sends_changed)
    {
        channel_screen_sends_compute(&views->sends, v.output_gain, send_level, send_pan);
        shown->fader = v.output_gain;
        memcpy(shown->send_level, send_level, sizeof(send_level));
        memcpy(shown->send_pan, send_pan, sizeof(send_pan));
    }

    // first time, or mute or solo changed: the background colour changes, so draw the whole screen
    if (!shown->drawn || now.muted != was->muted || now.soloed != was->soloed)
    {
#if DEVELOPER_MODE
        if (!shown->drawn) printf("\r\nChannel display %u: first draw\n", (unsigned)display);
#endif
        memcpy(shown->popup, popup, sizeof(popup));
        channel_screen_parts_t parts;
        channel_screen_full(&region, &parts, &now, views);
        displays_draw_region(display, &region);
        shown->drawn = true;
        *was = now;
        return;
    }

    // title strip: input type changed, or the selected channel's strip flashes
    if (now.input != was->input || now.title_lit != was->title_lit)
    {
        channel_header_parts_t parts;
        channel_screen_header(&region, &parts, &now);
        displays_draw_region(display, &region);
        was->input = now.input;
        was->title_lit = now.title_lit;
    }

    // value pop-up: appeared, moved, changed or went away
    const bool popup_changed = now.popup != was->popup || now.popup_rgb != was->popup_rgb || strcmp(popup, shown->popup) != 0;
    const channel_popup_where_t old_where = was->popup;
    memcpy(shown->popup, popup, sizeof(popup));
    was->popup = now.popup;
    was->popup_rgb = now.popup_rgb;
    if (popup_changed && (old_where == CH_POPUP_TOP || now.popup == CH_POPUP_TOP))
    {
        channel_bar_parts_t parts;
        channel_screen_bar_slot(&region, &parts, views, &now);
        displays_draw_region(display, &region);
    }

    // switched between EQ, sends and compressor, or what the middle shows changed
    if (now.show_eq != was->show_eq || now.show_sends != was->show_sends || (now.show_eq && eq_changed) ||
        (now.show_sends && sends_changed))
    {
        channel_popup_parts_t parts;
        channel_screen_middle(&region, &parts, views, &now);
        displays_draw_region(display, &region);
        was->show_eq = now.show_eq;
        was->show_sends = now.show_sends;
    }
    if (popup_changed && (old_where == CH_POPUP_LOW || now.popup == CH_POPUP_LOW))
    {
        channel_popup_parts_t parts;
        channel_screen_low_popup_area(&region, &parts, views, &now);
        displays_draw_region(display, &region);
    }

    if (now.pan_fill != was->pan_fill)
    {
        channel_pan_parts_t parts;
        channel_screen_pan(&region, &parts, now.pan_fill);
        displays_draw_region(display, &region);
        was->pan_fill = now.pan_fill;
    }
}

static void channel_display_task (void *argument)
{
    (void)argument;
    for (;;)
    {
        const bool flash_on = ((xTaskGetTickCount() / pdMS_TO_TICKS(SELECT_FLASH_MS)) & 1U) != 0U;
        for (uint8_t display = 0; display < CHANNELS; display++)
        {
            update_display(display, flash_on);
        }
        osDelay(pdMS_TO_TICKS(CHANNEL_DISPLAY_PERIOD_MS));
    }
}

static uint32_t ChannelDisplayStack[768];
static StaticTask_t ChannelDisplayControlBlock;

void init_channel_display (void)
{
    static const osThreadAttr_t attr = {
        .name = "displays",
        .stack_mem = ChannelDisplayStack,
        .stack_size = sizeof(ChannelDisplayStack),
        .cb_mem = &ChannelDisplayControlBlock,
        .cb_size = sizeof(ChannelDisplayControlBlock),
        // One step above the I2C, LED and fader tasks (osPriorityBelowNormal). The I2C transfers poll
        // their flags and keep the processor busy, so at their level drawing only got a share of it.
        // Above them a redraw finishes in one go; with the faster bus that's a few milliseconds.
        .priority = osPriorityBelowNormal1,
    };
    if (osThreadNew(channel_display_task, NULL, &attr) == NULL)
    {
        printf("\r\nChannel displays task FAILED to start\n");
        return;
    }
    printf("\r\nChannel displays task started\n");
}