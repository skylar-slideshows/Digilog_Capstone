/**
  **********************************************************************************
  * BUTTON CONTROL - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/button_control.c
  * @brief The bucket's button handling (button_control.c), for the master's thirteen buttons:
  *        - INSERT: toggles the mix insert (lit while in)
  *        - PLAY, STOP, REC, FRW, FF: pressed and released straight to the DAW as Mackie
  *          Control (FRW and FF are MCU REWIND and FAST FWD: the DAW winds while they're
  *          held); their lights follow what the DAW says
  *        - B0-B3: the screen's buttons, counted for the screen; lit while held
  *        - B4-B6: no function yet, each toggles a flag and its light
  **********************************************************************************
*/

#include "button_control.h"
#include "control_internal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "control_integration/control_interface.h"
#include "FreeRTOS_apps/daw_control.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_sets.h"
#include <stddef.h>

#define TRANSPORT_FLASH_MS 250U // paused (PLAY) or record armed (REC): lit this long, then dark this long

static void update_toggle_button_val_from_info (SemaphoreHandle_t *out_mutex, const button_info_t *info, bool *val_out)
{
    if (update_button(info) == BUTTON_NO_EVENT)
    {
        return;
    }

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    *val_out = !*val_out;
    xSemaphoreGive(*out_mutex);
}


/*=============================== TRANSPORT ================================*/
// Each press and each release goes to the DAW as it happens, so a DAW that acts on holds
// (shuttling while a button is down, say) gets them too.

/** What each of the panel's transport buttons sends (io->transport[] is in this order) */
static const daw_button_t TRANSPORT_SENDS[TRANSPORT_BUTTONS] = {
    DAW_PLAY,    // PLAY
    DAW_STOP,    // STOP
    DAW_RECORD,  // REC
    DAW_REWIND,  // FRW: MCU REWIND, held
    DAW_FORWARD, // FF:  MCU FAST FWD, held
};
enum { T_PLAY = 0, T_STOP, T_REC, T_FRW, T_FF };

static bool s_transport_held[TRANSPORT_BUTTONS]; // as last sent to the DAW (I2C task only)

static void update_transport_buttons (master_control_io_t *io)
{
    for (uint8_t b = 0; b < TRANSPORT_BUTTONS; b++)
    {
        const bool held = get_button_held(&(io->transport[b].button));
        if (held == s_transport_held[b]) continue;
        s_transport_held[b] = held;
        (void)daw_control_button(TRANSPORT_SENDS[b], held); // false if USB isn't connected: nothing to do
    }
}


/*=============================== VALUES ================================*/

void update_button_values (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io)
{
    // INSERT
    {
        xSemaphoreTake(*vals_mutex, portMAX_DELAY);
        const bool before = vals->insert;
        xSemaphoreGive(*vals_mutex);
        update_toggle_button_val_from_info(vals_mutex, &(io->insert_button.button), &(vals->insert));
        if (vals->insert != before) control_touched(TOUCHED_INSERT, 0); // only this task writes it
    }

    // B4-B6: no function yet
    for (uint8_t i = 0; i < USER_BUTTONS; i++)
    {
        update_toggle_button_val_from_info(vals_mutex, &(io->user_button[i].button), &(vals->user_button[i]));
    }

    // B0-B3: the screen's
    for (uint8_t i = 0; i < SCREEN_BUTTONS; i++)
    {
        if (update_button(&(io->screen_button[i].button)) != BUTTON_NO_EVENT) screen_input_button_pressed(i);
    }

    update_transport_buttons(io);
}


/*=============================== LEDS ================================*/

static void set_led (led_device_t *led, bool on)
{
    if (led != NULL) led_set_bool(led, on);
}

void update_button_leds (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io)
{
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const master_controls v = *vals;
    xSemaphoreGive(*vals_mutex);

    set_led(io->insert_button.led, v.insert);

    for (uint8_t i = 0; i < USER_BUTTONS; i++)
    {
        set_led(io->user_button[i].led, v.user_button[i]);
    }

    for (uint8_t i = 0; i < SCREEN_BUTTONS; i++)
    {
        set_led(io->screen_button[i].led, get_button_held(&(io->screen_button[i].button)));
    }

    // transport: from what the DAW says, not from the buttons
    bool armed = false;
    const daw_transport_t state = daw_control_transport(&armed);
    const bool flash_on = ((xTaskGetTickCount() / pdMS_TO_TICKS(TRANSPORT_FLASH_MS)) & 1U) != 0U;

    set_led(io->transport[T_PLAY].led, state == DAW_PLAYING || state == DAW_RECORDING || (state == DAW_PAUSED && flash_on));
    set_led(io->transport[T_STOP].led, state == DAW_STOPPED);
    set_led(io->transport[T_REC].led, state == DAW_RECORDING || (armed && flash_on));
    // rewind / fast forward: lit while held, or while the DAW says it's winding
    set_led(io->transport[T_FRW].led, get_button_held(&(io->transport[T_FRW].button)) || daw_control_button_lit(DAW_REWIND));
    set_led(io->transport[T_FF].led, get_button_held(&(io->transport[T_FF].button)) || daw_control_button_lit(DAW_FORWARD));
}


/*=============================== THE BUTTONS' WIRING ================================*/
/*
 * From the schematic (controls and leds sheets). Every button pulls its expander pin low while held.
 * The LED numbers are positions in led_driver.c's init_devices (U5 and U6 outputs).
 */
typedef struct
{
    uint8_t addr;      //!< expander
    MCP23017_Reg port; //!< MCP_GPIOA / MCP_GPIOB
    uint8_t pin;
    uint8_t led;       //!< led_device_at() index
} button_wiring_t;

static const button_wiring_t SCREEN_BUTTON_WIRING[SCREEN_BUTTONS] = {
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 6, 5}, // B0  SW8:  U8 GPB6, LED B0_L U5 QB
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 5, 6}, // B1  SW9:  U8 GPB5, LED B1_L U5 QC
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 4, 7}, // B2  SW10: U8 GPB4, LED B2_L U5 QD
    {MCP23017_KNOBS_ADDR, MCP_GPIOA, 0, 4}, // B3  SW11: U7 GPA0, LED B3_L U5 QA
};

static const button_wiring_t USER_BUTTON_WIRING[USER_BUTTONS] = {
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 3, 14}, // B4  SW12: U8 GPB3, LED B4_L U6 QC
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 2, 13}, // B5  SW13: U8 GPB2, LED B5_L U6 QB
    {MCP23017_PANEL_ADDR, MCP_GPIOB, 1, 12}, // B6  SW14: U8 GPB1, LED B6_L U6 QA
};

static const button_wiring_t TRANSPORT_WIRING[TRANSPORT_BUTTONS] = {
    [T_PLAY] = {MCP23017_PANEL_ADDR, MCP_GPIOA, 2, 18}, // PLAY SW15 (B7):  U8 GPA2, LED B7_L U6 QG
    [T_STOP] = {MCP23017_PANEL_ADDR, MCP_GPIOA, 1, 19}, // STOP SW16 (B8):  U8 GPA1, LED B8_L U6 QH
    [T_REC]  = {MCP23017_PANEL_ADDR, MCP_GPIOA, 3, 17}, // REC  SW17 (B9):  U8 GPA3, LED B9_L U6 QF
    [T_FRW]  = {MCP23017_PANEL_ADDR, MCP_GPIOA, 4, 16}, // FRW  SW18 (B10): U8 GPA4, LED B10_L U6 QE
    [T_FF]   = {MCP23017_PANEL_ADDR, MCP_GPIOA, 5, 15}, // FF   SW19 (B11): U8 GPA5, LED B11_L U6 QD
};

static const button_wiring_t INSERT_WIRING = {MCP23017_KNOBS_ADDR, MCP_GPIOA, 1, 11}; // SW20: U7 GPA1, LED INSERT_L U5 QH

static void init_lit_button (lit_button_info_t *b, const button_wiring_t *w)
{
    b->button = (button_info_t){
        .bus = CONTROLS_I2C_BUS,
        .addr = w->addr,
        .port = w->port,
        .pin = w->pin,
        .active_low = true,
    };
    b->led = led_device_at(w->led);
}

void init_button_controls (master_control_io_t *io)
{
    for (uint8_t i = 0; i < SCREEN_BUTTONS; i++) init_lit_button(&(io->screen_button[i]), &SCREEN_BUTTON_WIRING[i]);
    for (uint8_t i = 0; i < USER_BUTTONS; i++) init_lit_button(&(io->user_button[i]), &USER_BUTTON_WIRING[i]);
    for (uint8_t i = 0; i < TRANSPORT_BUTTONS; i++) init_lit_button(&(io->transport[i]), &TRANSPORT_WIRING[i]);
    init_lit_button(&(io->insert_button), &INSERT_WIRING);
}
