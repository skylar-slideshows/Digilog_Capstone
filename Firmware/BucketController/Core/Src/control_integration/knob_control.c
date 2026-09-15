#include "control_integration/control_interface.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "hardware_structs.h"

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

// helpers for less code
static inline void knob_info_disp(knob_info_t knob, knob_disp_t mode){
    knob_disp(knob.led_ring.channel, knob.led_ring.knob_num, mode);
}

static inline void knob_info_scale(knob_info_t knob, knob_scale_t mode){
    knob_scale(knob.led_ring.channel, knob.led_ring.knob_num, mode);
}

static inline void knob_info_led(knob_info_t knob, uint8_t value){
    knob_led(knob.led_ring.channel, knob.led_ring.knob_num, value);
}

static inline uint8_t uscalar_to_8bit(u_scalar_control_t in){
    return (uint8_t)(in << (sizeof(u_scalar_control_t) * 8 - 8));
}

void update_channel_knob_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io)
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

void update_channel_knob_outputs (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io)
{
    // TODO: Update LED rings
    knob_info_led(io->input_gain_knob, uscalar_to_8bit(vals->input_gain));
    knob_info_led(io->hf_interface.gain_knob, uscalar_to_8bit(vals->hf_control.gain));
}

void init_knob_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{
    // TODO: per-channel config stuff (probably just choosing i2c bus based on channel arg)

    button_info_t input_gain_button = {.bus = I2C1, .addr = 0x60, .port = MCP_GPIOA, .pin = 0}; // dummy
    io->input_gain_knob.encoder = (encoder_info_t){.i2c_bus = I2C1,
                                                   .i2c_addr = 0x20,
                                                   .a_register = MCP_GPIOB,
                                                   .a_pin = 0,
                                                   .b_register = MCP_GPIOB,
                                                   .b_pin = 1,
                                                   .button_info = input_gain_button};
    get_encoder_motion(io->input_gain_knob.encoder, state->input_gain_encoder_state, &(state->input_gain_encoder_state));
    io->input_gain_knob.led_ring = (led_ring_info_t){.channel = 0, .knob_num = 0};
    knob_info_scale(io->input_gain_knob, SCALE_LEFT);
    knob_info_disp(io->input_gain_knob, 0);

    button_info_t hf_gain_button = {.bus = I2C1, .addr = 0x60, .port = MCP_GPIOA, .pin = 0}; // dummy
    io->hf_interface.gain_knob.encoder = (encoder_info_t){.i2c_bus = I2C1,
                                                          .i2c_addr = 0x20,
                                                          .a_register = MCP_GPIOB,
                                                          .a_pin = 2,
                                                          .b_register = MCP_GPIOB,
                                                          .b_pin = 3,
                                                          .button_info = hf_gain_button};
    get_encoder_motion(
        io->hf_interface.gain_knob.encoder,
        state->hf_interface_state.gain_encoder_state,
        &(state->hf_interface_state.gain_encoder_state)
    );
    io->hf_interface.gain_knob.led_ring = (led_ring_info_t){.channel = 0, .knob_num = 0};
    knob_info_scale(io->hf_interface.gain_knob, SCALE_LEFT);
    knob_info_disp(io->hf_interface.gain_knob, 0);

    // TODO: Fill channel_controls_io to match the hardware, and set initial states
}
