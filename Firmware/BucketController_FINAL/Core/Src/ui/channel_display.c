/**
  **********************************************************************************
  * CHANNEL DISPLAYS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/channel_display.c
  * @brief See channel_display.h. What each screen shows is laid out in ui/channel_screen.c;
  *        displays.c sends it to the panels.
  *
  * The panels' bus is slow, so nothing is redrawn unless it changed: the whole screen
  * when mute changes (the background colour changes), the EQ graph when an EQ setting
  * changes, and the pan bar when the pan moves by at least a pixel.
  **********************************************************************************
*/

#include "FreeRTOS_apps/channel_display.h"
#include "FreeRTOS_apps/displays.h"
#include "ui/channel_screen.h"
#include "control_integration/control_interface.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include <stdbool.h>
#include <stdio.h>

#define CHANNEL_DISPLAY_PERIOD_MS 33U // about 30 checks a second

_Static_assert(CHANNELS <= DISPLAY_COUNT, "one screen per channel");

/** The channel each screen shows (screen 0 shows channel 0, labelled 01). Reorder if the screens are mounted differently. */
static const uint8_t DISPLAY_CHANNEL[CHANNELS] = {0, 1, 2, 3};

/** What is on each screen now, so only changes get redrawn */
typedef struct
{
    bool drawn;       //!< false until the screen has been drawn once
    bool muted;
    int16_t pan_fill; //!< columns of the pan bar filled, from channel_screen_pan_fill()
    channel_eq_t eq;
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

static void update_display (uint8_t display)
{
    const uint8_t channel = DISPLAY_CHANNEL[display];

    // copy what this screen needs while holding the channel's lock, then draw without it
    xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
    const channel_controls *vals = &channel_control_vals[channel];
    const bool muted = vals->muted;
    const int16_t pan = vals->output_pan;
    const channel_eq_t eq = {
        .hf = eq_band(&vals->hf_control),
        .hmf = eq_band(&vals->hmf_control),
        .lmf = eq_band(&vals->lmf_control),
        .lf = eq_band(&vals->lf_control),
        .hpf = vals->high_pass_filter,
    };
    xSemaphoreGive(channel_control_mutexes[channel]);

    const int16_t pan_fill = channel_screen_pan_fill(pan);
    shown_t *shown = &s_shown[display];
    ui_region_t region;

    // first time, or mute changed: the background colour changes, so draw the whole screen
    if (!shown->drawn || muted != shown->muted)
    {
#if DEVELOPER_MODE
        if (!shown->drawn) printf("\r\nChannel display %u: first draw\n", (unsigned)display);
#endif
        channel_screen_parts_t parts;
        channel_screen_eq_compute(&s_eq_view, &eq);
        channel_screen_full(&region, &parts, (uint8_t)(channel + 1U), muted, pan_fill, &s_eq_view);
        displays_draw_region(display, &region);
        *shown = (shown_t){.drawn = true, .muted = muted, .pan_fill = pan_fill, .eq = eq};
        return;
    }

    if (!same_eq(&eq, &shown->eq))
    {
        channel_screen_eq_compute(&s_eq_view, &eq);
        channel_screen_eq(&region, &s_eq_view, muted);
        displays_draw_region(display, &region);
        shown->eq = eq;
    }

    if (pan_fill != shown->pan_fill)
    {
        channel_pan_parts_t parts;
        channel_screen_pan(&region, &parts, pan_fill);
        displays_draw_region(display, &region);
        shown->pan_fill = pan_fill;
    }
}

static void channel_display_task (void *argument)
{
    (void)argument;
    for (;;)
    {
        for (uint8_t display = 0; display < CHANNELS; display++)
        {
            update_display(display);
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
        // Same priority as the I2C, LED and fader tasks. The I2C transfers poll their flags and only
        // yield to tasks of equal or higher priority, so a lower-priority task here would never run.
        .priority = osPriorityBelowNormal,
    };
    if (osThreadNew(channel_display_task, NULL, &attr) == NULL)
    {
        printf("\r\nChannel displays task FAILED to start\n");
        return;
    }
    printf("\r\nChannel displays task started\n");
}
