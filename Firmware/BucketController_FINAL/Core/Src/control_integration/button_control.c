#include "button_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "portmacro.h"

static void update_toggle_button_val_from_info (
    SemaphoreHandle_t *out_mutex,
    button_state_t *btn_state,
    button_info_t *info,
    bool *val_out //
)
{
    bool new_button_state = get_button_held(info);

    if (new_button_state && !(btn_state->held))
    {
        xSemaphoreTake(*out_mutex, portMAX_DELAY);
        *val_out = !*val_out;
        xSemaphoreGive(*out_mutex);
        btn_state->press_countdown = DOUBLE_PRESS_TIME;
    }

    btn_state->held = new_button_state;
}

static inline void update_button_timer (button_state_t *btn_state)
{
    if (btn_state->press_countdown == 0)
    {
        return;
    }
    btn_state->press_countdown--;
}

static void update_radio_buttons (channel_control_io_state *state, channel_control_io_t *io)
{
    update_button_timer(&(state->send_button_state));
    update_button_timer(&(state->eq_button_state));
    update_button_timer(&(state->comp_button_state));

    bool send_button_held = get_button_held(&(io->send_button.button));
    bool eq_button_held = get_button_held(&(io->eq_button.button));
    bool comp_button_held = get_button_held(&(io->comp_button.button));

    if (send_button_held)
    {
        state->control_set_selection = SENDS_CONTROL;
    }
    else if (eq_button_held)
    {
        state->control_set_selection = EQ_CONTROL;
    }
    else if (comp_button_held)
    {
        state->control_set_selection = COMP_CONTROL;
    }
}

void init_button_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{
    // TODO: Fill channel_controls_io to match the hardware, and set initial states
}

void update_channel_button_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    // NOTE: check state->control_set_selection (sends, eq, or comp enum) to check what control set is selected by the radio buttons
    // (see update_radio_buttons)
}

void update_channel_button_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    update_radio_buttons(state, io);

    update_button_timer(&(state->mute_button_state));
    update_toggle_button_val_from_info(vals_mutex, &(state->mute_button_state), &(io->mute_button.button), &(vals->muted));

    // NOTE: check state->control_set_selection (sends, eq, or comp enum) to check what control set is selected by the radio buttons
    // (see update_radio_buttons)

    // TODO: Do this for the rest of the toggle-buttons on the channel
    // TODO: handle non-toggle (e.g held) buttons
}
