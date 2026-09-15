#include "control_integration/control_interface.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"

/* Functions related to accepting some states and updating control values */

// Use negative sensitivity for backwards turning
static void update_u_value_from_encoder_motion (
    encoder_state_t *state,
    encoder_info_t *info,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    encoder_turn_action_t motion = get_encoder_motion(*info, *state, state);
    if (motion == ENCODER_TURN_B)
    {
        sensitivity = -sensitivity;
    }
    else if (motion != ENCODER_TURN_A)
    {
        return;
    }

    if (sensitivity > 0 && U_SCALAR_CONTROL_MAX - *out < (uint32_t)sensitivity)
    {
        *out = U_SCALAR_CONTROL_MAX;
    }
    else if (sensitivity < 0 && (uint32_t)(-sensitivity) > *out)
    {
        *out = 0;
    }
    else
    {
        *out += sensitivity;
    }
}

static void update_s_value_from_encoder_motion (
    encoder_state_t *state,
    encoder_info_t *info,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    encoder_turn_action_t motion = get_encoder_motion(*info, *state, state);
    if (motion == ENCODER_TURN_B)
    {
        sensitivity = -sensitivity;
    }
    else if (motion != ENCODER_TURN_A)
    {
        return;
    }

    if (sensitivity > 0 && *out > S_SCALAR_CONTROL_MAX - sensitivity)
    {
        *out = S_SCALAR_CONTROL_MAX;
    }
    else if (sensitivity < 0 && *out < S_SCALAR_CONTROL_MIN - sensitivity)
    {
        *out = S_SCALAR_CONTROL_MIN;
    }
    else
    {
        *out += sensitivity;
    }
}

void update_channel_encoder_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io)
{
    s_scalar_control_t default_sensitivity = S_SCALAR_CONTROL_MAX / 16;

    update_s_value_from_encoder_motion(
        &(state->input_gain_encoder_state),
        &(io->input_gain_knob.encoder),
        &(vals->input_gain),
        get_button_state(&io->input_gain_knob.encoder.button_info) ? default_sensitivity / 2 : default_sensitivity
    );

    // TODO: Do this for the rest of the encoders on the channel
}
