#include "control_integration/control_interface.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "hardware_structs.h"
#include "portmacro.h"
#include <stdbool.h>

/* Functions related to accepting some states and updating control values */

static void s_update_knob_button (
    SemaphoreHandle_t *out_mutex,
    encoder_state_t *state,
    encoder_info_t *info,
    s_scalar_control_t *out //
)
{
    const bool held = !get_button_held(&(info->button_info));
    const bool pressed = held & !(state->button_state.held);

    state->button_state.held = held;

    if (state->button_state.press_countdown > 0)
    {
        state->button_state.press_countdown--;
    }

    if (!pressed)
    {
        return;
    }

    if (state->button_state.press_countdown == 0)
    {
        // single press
        state->button_state.press_countdown = DOUBLE_PRESS_TIME;
        return;
    }

    // Double press
    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    *out = 0;
    xSemaphoreGive(*out_mutex);
}

static void u_update_knob_button (
    SemaphoreHandle_t *out_mutex,
    encoder_state_t *state,
    encoder_info_t *info,
    u_scalar_control_t *out //
)
{
    // Should be fine to pass it as a pointer like this; 0 is the same signed vs unsigned
    s_update_knob_button(out_mutex, state, info, (s_scalar_control_t *)out);
}


// Use negative sensitivity for backwards turning
static void update_u_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    encoder_state_t *state,
    encoder_info_t *info,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    u_update_knob_button(out_mutex, state, info, out);

    if (state->button_state.held)
    {
        sensitivity /= 2;
    }

    encoder_turn_action_t motion = get_encoder_motion(*info, *state, state);
    if (motion == ENCODER_TURN_B)
    {
        sensitivity = -sensitivity;
    }
    else if (motion != ENCODER_TURN_A)
    {
        return;
    }

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (sensitivity > 0 && U_SCALAR_CONTROL_MAX - *out < (u_scalar_control_t)sensitivity)
    {
        *out = U_SCALAR_CONTROL_MAX;
    }
    else if (sensitivity < 0 && (u_scalar_control_t)(-sensitivity) > *out)
    {
        *out = 0;
    }
    else
    {
        *out += sensitivity;
    }
    xSemaphoreGive(*out_mutex);
}

static void update_s_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    encoder_state_t *state,
    encoder_info_t *info,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    s_update_knob_button(out_mutex, state, info, out);

    if (state->button_state.held)
    {
        sensitivity /= 2;
    }

    encoder_turn_action_t motion = get_encoder_motion(*info, *state, state);
    if (motion == ENCODER_TURN_B)
    {
        sensitivity = -sensitivity;
    }
    else if (motion != ENCODER_TURN_A)
    {
        return;
    }

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
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
    xSemaphoreGive(*out_mutex);
}

void update_channel_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    s_scalar_control_t default_sensitivity = S_SCALAR_CONTROL_MAX / 32;

    update_u_value_from_encoder(
        vals_mutex,
        &(state->input_gain_encoder_state),
        &(io->input_gain_knob.encoder),
        &(vals->input_gain),
        default_sensitivity
    );
    update_s_value_from_encoder(
        vals_mutex,
        &(state->hf_interface_state.gain_encoder_state),
        &(io->hf_interface.gain_knob.encoder),
        &(vals->hf_control.gain),
        default_sensitivity
    );

    // TODO: Do this for the rest of the encoders on the channel
}

void update_channel_knob_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    // begin skylar [edit 2/2]
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const u_scalar_control_t input_gain = vals->input_gain;
    const s_scalar_control_t hf_gain = vals->hf_control.gain;
    xSemaphoreGive(*vals_mutex);

    // we will need one for each led ring plus logic to display the knob's alternate parameter if it is one with push = other param
    led_set(io->input_gain_knob.led_ring, input_gain);
    led_set_signed(io->hf_interface.gain_knob.led_ring, hf_gain);

    // end skylar [edit 2/2]
}

void init_knob_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{
    // TODO: per-channel config stuff (probably just choosing i2c bus based on channel arg)

    // INPUT GAIN KNOB CHANNEL 1
    button_info_t input_gain_button = {.bus = I2C1, .addr = 0x20, .port = MCP_GPIOB, .pin = 2}; // dummy
    io->input_gain_knob.encoder = (encoder_info_t){.i2c_bus = I2C1,
                                                   .i2c_addr = 0x20,
                                                   .a_register = MCP_GPIOB,
                                                   .a_pin = 1,
                                                   .b_register = MCP_GPIOB,
                                                   .b_pin = 0,
                                                   .button_info = input_gain_button};
    get_encoder_motion(io->input_gain_knob.encoder, state->input_gain_encoder_state, &(state->input_gain_encoder_state));
    state->input_gain_encoder_state.button_state = (button_state_t){.held = false, .press_countdown = 0};

    // HF GAIN KNOB CHANNEL 1
    button_info_t hf_gain_button = {.bus = I2C1, .addr = 0x20, .port = MCP_GPIOB, .pin = 5}; // dummy
    io->hf_interface.gain_knob.encoder = (encoder_info_t){.i2c_bus = I2C1,
                                                          .i2c_addr = 0x20,
                                                          .a_register = MCP_GPIOB,
                                                          .a_pin = 4,
                                                          .b_register = MCP_GPIOB,
                                                          .b_pin = 3,
                                                          .button_info = hf_gain_button};
    get_encoder_motion(
        io->hf_interface.gain_knob.encoder,
        state->hf_interface_state.gain_encoder_state,
        &(state->hf_interface_state.gain_encoder_state)
    );
    state->hf_interface_state.gain_encoder_state.button_state = (button_state_t){.held = false, .press_countdown = 0};

    // TODO: Fill channel_controls_io to match the hardware, and set initial states

    //begin skylar [edit 1/2]

    // this is how to link the button info objects with the led devices
    io->input_gain_knob.led_ring = led_device_at(4);
    io->hf_interface.gain_knob.led_ring = led_device_at(5);

    //end skylar [edit 1/2]
}
