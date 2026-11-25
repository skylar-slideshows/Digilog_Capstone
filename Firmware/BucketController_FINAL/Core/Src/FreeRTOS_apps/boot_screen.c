/**
  **********************************************************************************
  * START-UP SCREEN - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/boot_screen.c
  * @brief See boot_screen.h.
  **********************************************************************************
*/

#include "FreeRTOS_apps/boot_screen.h"
#include "FreeRTOS_apps/channel_display.h"
#include "FreeRTOS_apps/displays.h"
#include "FreeRTOS_apps/master_link.h"
#include "hardware_drivers/led_driver.h"
#include "console_state_bucket.h"
#include "ui/digilog.h" // the picture (included here only: it's 86 kB of flash)
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <stdio.h>

#define GUI_READY_WAIT_MS 1000U // longest wait for the channel screens' first full frames

static volatile bool s_done;
static uint8_t s_led_brightness; // the LEDs' normal brightness, set in main before this starts

static uint32_t s_stack[512];
static StaticTask_t s_tcb;

/**
 * The screens' backlight from one level to another over ms (the knob LEDs aren't faded). The levels are how
 * bright it looks, 0-1000; the PWM duty is that squared, so the fade looks even to the eye.
 */
static void fade_screens (uint16_t from, uint16_t to, uint32_t ms)
{
    const TickType_t start = xTaskGetTickCount();
    for (;;)
    {
        uint32_t t = (uint32_t)(xTaskGetTickCount() - start);
        if (t > ms) t = ms;
        const int32_t look = (int32_t)from + ((int32_t)to - (int32_t)from) * (int32_t)t / (int32_t)(ms ? ms : 1U);
        displays_backlight((uint16_t)((uint32_t)(look * look) / 1000U));
        if (t >= ms) return;
        osDelay(10);
    }
}

static void boot_task (void *argument)
{
    (void)argument;

    // 1. the picture, the screens fading in (the knob LEDs stay dark)
    for (uint8_t d = 0; d < DISPLAY_COUNT; d++) displays_draw_image(d, 0, 0, &digilog);
    fade_screens(0, 1000, BOOT_FADE_IN_MS);

    // 2. until the master has given us our channels, or BOOT_MASTER_TIMEOUT_MS after start-up
    while (!console_state_known() && xTaskGetTickCount() < pdMS_TO_TICKS(BOOT_MASTER_TIMEOUT_MS)) osDelay(20);
    if (!console_state_known())
    {
        // channels 01-04, NO MASTER along the bottom of the screens. The link keeps saying hello, so a
        // master that starts later still finds this bucket, and its %base% ends solo mode.
        console_state_set_solo();
    }

    // 3. the picture fades out
    fade_screens(1000, 0, BOOT_FADE_OUT_MS);

    // 4. the channel screens are drawn while dark, then the knob LEDs come straight on and the screens fade in
    s_done = true;
    for (uint32_t waited = 0; !channel_display_ready() && waited < GUI_READY_WAIT_MS; waited += 10U) osDelay(10);
    led_brightness(s_led_brightness);
    fade_screens(0, 1000, BOOT_FADE_OUT_MS);
    osThreadExit();
}

void boot_screen_init (void)
{
    s_led_brightness = led_get_brightness();
    led_brightness(0); // knob LEDs dark until the control surface is up
    displays_backlight(0);

    static const osThreadAttr_t attr = {
        .name = "boot_screen",
        .priority = osPriorityNormal,
        .stack_mem = s_stack,
        .stack_size = sizeof(s_stack),
        .cb_mem = &s_tcb,
        .cb_size = sizeof(s_tcb),
    };
    osThreadNew(boot_task, NULL, &attr);
}

bool boot_screen_done (void)
{
    return s_done;
}
