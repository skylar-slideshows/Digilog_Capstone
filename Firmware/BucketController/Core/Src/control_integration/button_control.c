#include "control_integration/control_interface.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"

static void update_toggle_button_val_from_info (bool *btn_state, button_info_t *info, bool *val_out)
{
    bool new_button_state = get_button_state(info);

    if (new_button_state && !*btn_state)
    {
        *val_out = !*val_out;
    }

    *btn_state = new_button_state;
}

void update_channel_button_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io)
{
    update_toggle_button_val_from_info(&(state->mute_button_state), &(io->mute_button.button), &(vals->muted));

    // TODO: Do this for the rest of the toggle-buttons on the channel
    // TODO: handle non-toggle (e.g radio) buttons
}
