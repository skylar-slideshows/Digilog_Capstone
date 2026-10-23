#include "button_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_sets.h"
#include "portmacro.h"

static void update_toggle_button_val_from_info (
    SemaphoreHandle_t *out_mutex,
    const button_info_t *info,
    bool *val_out //
)
{
    if (update_button(info) == BUTTON_NO_EVENT)
    {
        return;
    }

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    *val_out = !*val_out;
    xSemaphoreGive(*out_mutex);
}

static void update_radio_buttons (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    channel_control_set_selection_t selection;
    if (get_button_held(&(io->send_button.button)))
    {
        selection = SENDS_CONTROL;
    }
    else if (get_button_held(&(io->eq_button.button)))
    {
        selection = EQ_CONTROL;
    }
    else if (get_button_held(&(io->comp_button.button)))
    {
        selection = COMP_CONTROL;
    }
    else
    {
        return;
    }

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    vals->control_set_selection = selection;
    xSemaphoreGive(*vals_mutex);
}

void init_button_controls (uint8_t channel, channel_control_io_t *io)
{
    // TODO: Fill channel_controls_io to match the hardware
}

void update_channel_button_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    // NOTE: check vals->control_set_selection (sends, eq, or comp enum) to check what control set is selected by the radio buttons
    // (see update_radio_buttons)
}

void update_channel_button_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    update_radio_buttons(vals_mutex, vals, io);

    update_toggle_button_val_from_info(vals_mutex, &(io->mute_button.button), &(vals->muted));

    // NOTE: check vals->control_set_selection (sends, eq, or comp enum) to check what control set is selected by the radio buttons
    // (see update_radio_buttons)

    // TODO: Do this for the rest of the toggle-buttons on the channel
    // TODO: handle non-toggle (e.g held) buttons
}
