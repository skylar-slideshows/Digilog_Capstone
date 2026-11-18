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

#define CHANNEL_DISPLAY_PERIOD_MS 33U  // about 30 checks a second
#define SELECT_FLASH_MS           500U // selected channel: title strip lighter for this long, then normal for this long

_Static_assert(CHANNELS <= DISPLAY_COUNT, "one screen per channel");

/** The channel each screen shows (screen 0 shows channel 0, labelled 01). Reorder if the screens are mounted differently. */
static const uint8_t DISPLAY_CHANNEL[CHANNELS] = {0, 1, 2, 3};

/** What is on each screen now, so only changes get redrawn */
typedef struct
{
    bool drawn;                   //!< false until the screen has been drawn once
    channel_screen_state_t state;
    channel_eq_t eq;              //!< the settings the graph on screen was drawn from (if state.show_eq)
} shown_t;

static shown_t s_shown[CHANNELS];
static channel_eq_view_t s_eq_view; // worked-out EQ graph, reused for each screen (only this task draws)

static channel_eq_band_t eq_band (const eq_band_control_t *band)
{
    return (channel_eq_band_t){.gain = band->gain, .q = band->q, .freq = band->freq};
}

static bool same_band (const channel_eq_band_t *a, const channel_eq_band_t *b)
{
    return a->gain == b->gain && a->q == b->q && a->freq == b->freq;
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

static void update_display (uint8_t display, bool flash_on)
{
    const uint8_t channel = DISPLAY_CHANNEL[display];

    // copy what this screen needs while holding the channel's lock, then draw without it
    xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
    const channel_controls *vals = &channel_control_vals[channel];
    const channel_screen_state_t now = {
        .number = (uint8_t)(channel + 1U),
        .input = input_type(vals->input_type_selection),
        .muted = vals->muted,
        .soloed = vals->solo,
        .title_lit = vals->sel && flash_on,
        .show_eq = (vals->control_set_selection == EQ_CONTROL),
        .pan_fill = channel_screen_pan_fill(vals->output_pan),
    };
    const channel_eq_t eq = {
        .hf = eq_band(&vals->hf_control),
        .hmf = eq_band(&vals->hmf_control),
        .lmf = eq_band(&vals->lmf_control),
        .lf = eq_band(&vals->lf_control),
        .hpf = vals->high_pass_filter,
    };
    xSemaphoreGive(channel_control_mutexes[channel]);

    shown_t *shown = &s_shown[display];
    channel_screen_state_t *was = &shown->state;
    ui_region_t region;

    // first time, or mute or solo changed: the background colour changes, so draw the whole screen
    if (!shown->drawn || now.muted != was->muted || now.soloed != was->soloed)
    {
#if DEVELOPER_MODE
        if (!shown->drawn) printf("\r\nChannel display %u: first draw\n", (unsigned)display);
#endif
        channel_screen_parts_t parts;
        if (now.show_eq) channel_screen_eq_compute(&s_eq_view, &eq);
        channel_screen_full(&region, &parts, &now, &s_eq_view);
        displays_draw_region(display, &region);
        *shown = (shown_t){.drawn = true, .state = now, .eq = eq};
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

    // switched to or from the EQ controls, or an EQ setting changed while the graph is showing
    if (now.show_eq != was->show_eq || (now.show_eq && !same_eq(&eq, &shown->eq)))
    {
        if (now.show_eq) channel_screen_eq_compute(&s_eq_view, &eq);
        channel_screen_eq(&region, &s_eq_view, &now);
        displays_draw_region(display, &region);
        was->show_eq = now.show_eq;
        shown->eq = eq;
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