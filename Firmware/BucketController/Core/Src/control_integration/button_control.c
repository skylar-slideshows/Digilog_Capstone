#include "button_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "portmacro.h"

static void
update_toggle_button_val_from_info (SemaphoreHandle_t *out_mutex, bool *btn_state, button_info_t *info, bool *val_out)
{
    bool new_button_state = get_button_state(info);

    if (new_button_state && !*btn_state)
    {
        xSemaphoreTake(*out_mutex, portMAX_DELAY);
        *val_out = !*val_out;
        xSemaphoreGive(*out_mutex);
    }

    *btn_state = new_button_state;
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
}

void update_channel_button_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    update_toggle_button_val_from_info(vals_mutex, &(state->mute_button_state), &(io->mute_button.button), &(vals->muted));

    // TODO: Do this for the rest of the toggle-buttons on the channel
    // TODO: handle non-toggle (e.g radio) buttons
}
