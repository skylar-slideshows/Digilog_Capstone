#include "CONFIG.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "hardware_structs.h"
#include "portmacro.h"
#include "stm32g474xx.h"
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


static const button_info_t KNOB_BUTTONS[10] =
{
    {.addr = 0x20, .port = MCP_GPIOA, .pin = 5},
    {.addr = 0x20, .port = MCP_GPIOA, .pin = 6},
    {.addr = 0x20, .port = MCP_GPIOB, .pin = 5},
    {.addr = 0x20, .port = MCP_GPIOB, .pin = 6},
    {.addr = 0x21, .port = MCP_GPIOA, .pin = 5},
    {.addr = 0x21, .port = MCP_GPIOA, .pin = 6},
    {.addr = 0x21, .port = MCP_GPIOB, .pin = 5},
    {.addr = 0x21, .port = MCP_GPIOB, .pin = 6},
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 0},
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 1}
};

static encoder_info_t KNOB_ENCODERS[10] =
{
    {.i2c_addr = 0x20, .a_register = MCP_GPIOA, .a_pin = 0,
                    .b_register = MCP_GPIOB, .b_pin = 0, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20, .a_register = MCP_GPIOA, .a_pin = 1,
                    .b_register = MCP_GPIOB, .b_pin = 1, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20, .a_register = MCP_GPIOA, .a_pin = 2,
                    .b_register = MCP_GPIOB, .b_pin = 2, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20, .a_register = MCP_GPIOA, .a_pin = 3,
                    .b_register = MCP_GPIOB, .b_pin = 3, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20, .a_register = MCP_GPIOA, .a_pin = 4,
                    .b_register = MCP_GPIOB, .b_pin = 4, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x21, .a_register = MCP_GPIOA, .a_pin = 0,
                    .b_register = MCP_GPIOB, .b_pin = 0, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x21, .a_register = MCP_GPIOA, .a_pin = 1,
                    .b_register = MCP_GPIOB, .b_pin = 1, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x21, .a_register = MCP_GPIOA, .a_pin = 2,
                    .b_register = MCP_GPIOB, .b_pin = 2, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x21, .a_register = MCP_GPIOA, .a_pin = 3,
                    .b_register = MCP_GPIOB, .b_pin = 3, .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x21, .a_register = MCP_GPIOA, .a_pin = 4,
                    .b_register = MCP_GPIOB, .b_pin = 4, .button_info = KNOB_BUTTONS[0]}
};


void init_knob (I2C_TypeDef *bus, knob_info_t *knob, encoder_state_t *state, uint8_t index)
{
    knob->encoder = KNOB_ENCODERS[index];
    knob->encoder.i2c_bus = bus;
    get_encoder_motion(knob->encoder, *state, state);
    state->button_state = (button_state_t){.held = false, .press_countdown = 0};
}


void init_knob_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{

    I2C_TypeDef *bus = i2c_channel_bus(channel);

    // INPUT AND PAN KNOB
    init_knob(bus, &(io->input_gain_knob), &(state->input_gain_encoder_state), 0);
    init_knob(bus, &(io->pan), &(state->pan_knob_state), 9);

    // SENDS
    init_knob(bus, &(io->send_channel_interfaces[0].gain_knob), &(state->send_channel_interface_states[0].gain_encoder), 1);
    init_knob(bus, &(io->send_channel_interfaces[0].pan_knob), &(state->send_channel_interface_states[0].pan_encoder), 2);

    init_knob(bus, &(io->send_channel_interfaces[1].gain_knob), &(state->send_channel_interface_states[1].gain_encoder), 3);
    init_knob(bus, &(io->send_channel_interfaces[1].pan_knob), &(state->send_channel_interface_states[1].pan_encoder), 4);

    init_knob(bus, &(io->send_channel_interfaces[2].gain_knob), &(state->send_channel_interface_states[2].gain_encoder), 5);
    init_knob(bus, &(io->send_channel_interfaces[2].pan_knob), &(state->send_channel_interface_states[2].pan_encoder), 6);

    init_knob(bus, &(io->send_channel_interfaces[3].gain_knob), &(state->send_channel_interface_states[3].gain_encoder), 7);

    // EQ HF
    init_knob(bus, &(io->hf_interface.gain_knob), &(state->hf_interface_state.gain_encoder_state), 1);
    init_knob(bus, &(io->hf_interface.gain_knob), &(state->hf_interface_state.freq_encoder_state), 2);
    
    // EQ HMF
    init_knob(bus, &(io->hmf_interface.gain_knob), &(state->hmf_interface_state.gain_encoder_state), 3);
    init_knob(bus, &(io->hmf_interface.freq_knob), &(state->hmf_interface_state.freq_encoder_state), 4);
    init_knob(bus, &(io->hmf_interface.q_knob), &(state->hmf_interface_state.q_encoder_state), 3);

    // EQ LMF
    init_knob(bus, &(io->lmf_interface.gain_knob), &(state->lmf_interface_state.gain_encoder_state), 5);
    init_knob(bus, &(io->lmf_interface.freq_knob), &(state->lmf_interface_state.freq_encoder_state), 6);
    init_knob(bus, &(io->lmf_interface.q_knob), &(state->lmf_interface_state.q_encoder_state), 5);
    
    // EQ LF
    init_knob(bus, &(io->lf_interface.gain_knob), &(state->lf_interface_state.gain_encoder_state), 7);
    init_knob(bus, &(io->lf_interface.freq_knob), &(state->lf_interface_state.freq_encoder_state), 8);

    // COMP
    init_knob(bus, &(io->comp_interface.in_gain_knob), &(state->comp_interface_state.in_gain_encoder_state), 1);
    init_knob(bus, &(io->comp_interface.threshold_knob), &(state->comp_interface_state.threshold_encoder_state), 2);
    init_knob(bus, &(io->comp_interface.attack_time_knob), &(state->comp_interface_state.attack_time_encoder_state), 3);
    init_knob(bus, &(io->comp_interface.release_time_knob), &(state->comp_interface_state.release_time_encoder_state), 4);
    init_knob(bus, &(io->comp_interface.ratio_knob), &(state->comp_interface_state.ratio_encoder_state), 5);
    init_knob(bus, &(io->comp_interface.out_gain_knob), &(state->comp_interface_state.out_gain_encoder_state), 6);


    
    
    
    if(channel == 0)
    {
        // CHANNEL 0 KNOB LED RINGS - LINK BUTTON INFO OBJECTS w/ LED DEVICES
        io->input_gain_knob.led_ring = led_device_at(4);
        io->hf_interface.gain_knob.led_ring = led_device_at(5);
    }
    
    if(channel == 1)
    {
        // CHANNEL 1 KNOB LED RINGS - LINK BUTTON INFO OBJECTS w/ LED DEVICES
    }

    if(channel == 2)
    {
        // CHANNEL 2 KNOB LED RINGS - LINK BUTTON INFO OBJECTS w/ LED DEVICES
    }

    if(channel == 3)
    {
        // CHANNEL 3 KNOB LED RINGS - LINK BUTTON INFO OBJECTS w/ LED DEVICES
    }
    // this is how to link the button info objects with the led devices

    

    //end skylar [edit 1/2]
}
