/**
  **********************************************************************************
  * CONTROL INTERFACE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/control_interface.c
  * @brief See control_interface.h. Laid out like the bucket's: values and their mutex, the
  *        panel's hardware descriptions, defaults, and the two update entry points.
  **********************************************************************************
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"

#include "control_integration/control_interface.h"
#include "control_internal.h"
#include "button_control.h"
#include "hardware_sets.h"
#include "knob_control.h"


/*=============================== VALUES ================================*/

master_controls master_control_vals;
SemaphoreHandle_t master_control_mutex;
static StaticSemaphore_t s_mutex_mem;

/** The panel: every knob and button, with its LED */
static master_control_io_t master_controls_io;

static master_controls s_defaults; // filled once by fill_defaults(); the values start as a copy

const master_controls *control_defaults (void)
{
    return &s_defaults;
}

static void fill_defaults (master_controls *vals)
{
    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        vals->aux_return_level[ret] = AUX_RETURN_UNITY; // 0 dB
        vals->aux_return_to_mix[ret] = AUX_RETURN_TO_MIX_DEFAULT;
    }
    vals->insert = false;
    for (uint8_t i = 0; i < USER_BUTTONS; i++) vals->user_button[i] = false;
}

void init_control_interface (void)
{
    fill_defaults(&s_defaults);
    master_control_mutex = xSemaphoreCreateMutexStatic(&s_mutex_mem);

    init_button_controls(&master_controls_io); // after led_init(): the buttons' LEDs must exist
    init_knob_controls(&master_controls_io);

    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    master_control_vals = s_defaults;
    xSemaphoreGive(master_control_mutex);
}


/*=============================== HARDWARE -> VALUES (I2C task) ================================*/

/**
 * Updates control values based on current/previous (cached) input (button/rotary-encoder) states.
 * The GPIO expanders should have been polled to cache before running this.
 */
void update_control_values (void)
{
    update_knob_values(&master_control_mutex, &master_control_vals, &master_controls_io);
    update_button_values(&master_control_mutex, &master_control_vals, &master_controls_io);
}


/*=============================== VALUES -> LEDS (LED task) ================================*/

void update_control_leds (void)
{
    update_knob_leds(&master_control_mutex, &master_control_vals, &master_controls_io);
    update_button_leds(&master_control_mutex, &master_control_vals, &master_controls_io);
}


/*=============================== SCREEN INPUTS ================================*/
// Written by the I2C task, taken by the screen (later): a short critical section each way

static screen_input_t s_screen_input;

void screen_input_turned (int8_t steps)
{
    taskENTER_CRITICAL();
    const int32_t turns = (int32_t)s_screen_input.turns + steps;
    s_screen_input.turns = (int16_t)(turns > INT16_MAX ? INT16_MAX : turns < INT16_MIN ? INT16_MIN : turns);
    taskEXIT_CRITICAL();
}

void screen_input_knob_pressed (void)
{
    taskENTER_CRITICAL();
    if (s_screen_input.knob_presses < UINT8_MAX) s_screen_input.knob_presses++;
    taskEXIT_CRITICAL();
}

void screen_input_button_pressed (uint8_t button)
{
    if (button >= SCREEN_BUTTONS) return;
    taskENTER_CRITICAL();
    if (s_screen_input.button_presses[button] < UINT8_MAX) s_screen_input.button_presses[button]++;
    taskEXIT_CRITICAL();
}

void control_get_aux_returns (uint16_t level[AUX_RETURNS], bool to_mix[AUX_RETURNS])
{
    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < AUX_RETURNS; i++)
    {
        level[i] = master_control_vals.aux_return_level[i];
        to_mix[i] = master_control_vals.aux_return_to_mix[i];
    }
    xSemaphoreGive(master_control_mutex);
}

void control_set_aux_returns (const uint16_t level[AUX_RETURNS], const bool to_mix[AUX_RETURNS])
{
    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < AUX_RETURNS; i++)
    {
        master_control_vals.aux_return_level[i] = level[i];
        master_control_vals.aux_return_to_mix[i] = to_mix[i];
    }
    xSemaphoreGive(master_control_mutex);
}

void control_set_aux_level (uint8_t index, uint16_t level)
{
    if (index >= AUX_RETURNS) return;
    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    master_control_vals.aux_return_level[index] = level;
    xSemaphoreGive(master_control_mutex);
}

void control_set_aux_to_mix (uint8_t index, bool to_mix)
{
    if (index >= AUX_RETURNS) return;
    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    master_control_vals.aux_return_to_mix[index] = to_mix;
    xSemaphoreGive(master_control_mutex);
}

static volatile uint8_t s_screen_leds_on, s_screen_leds_flash;

void control_set_screen_leds (uint8_t on_mask, uint8_t flash_mask)
{
    s_screen_leds_on = on_mask;
    s_screen_leds_flash = flash_mask;
}

void screen_leds_get (uint8_t *on_mask, uint8_t *flash_mask)
{
    *on_mask = s_screen_leds_on;
    *flash_mask = s_screen_leds_flash;
}

void control_take_screen_input (screen_input_t *out)
{
    if (out == NULL) return;
    taskENTER_CRITICAL();
    *out = s_screen_input;
    memset(&s_screen_input, 0, sizeof(s_screen_input));
    taskEXIT_CRITICAL();
}


/*=============================== LAST TOUCHED ================================*/

static volatile uint8_t s_touched_what;  // touched_param_t << 4 | which, written before the tick
static volatile uint32_t s_touched_tick;

void control_touched (touched_param_t param, uint8_t which)
{
    s_touched_what = (uint8_t)((param << 4) | (which & 0x0FU));
    s_touched_tick = (uint32_t)xTaskGetTickCount();
}

touched_t control_last_touched (void)
{
    const uint8_t what = s_touched_what;
    const touched_t t = {.param = (touched_param_t)(what >> 4), .which = (uint8_t)(what & 0x0FU), .tick = s_touched_tick};
    return t;
}
