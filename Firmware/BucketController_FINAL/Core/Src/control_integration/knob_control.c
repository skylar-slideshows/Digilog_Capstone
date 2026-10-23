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

typedef struct
{
    int8_t direction; //!< +1 turned toward A, -1 turned toward B, 0 not turned
    bool held;        //!< Push button is held (fine adjustment)
    bool reset;       //!< Push button was double pressed (reset value)
} knob_input_t;

static knob_input_t read_knob (const encoder_info_t *info)
{
    knob_input_t input = {0};

    input.reset = update_button(&(info->button_info)) == BUTTON_DOUBLE_PRESSED;
    input.held = get_button_held(&(info->button_info));

    const encoder_turn_action_t motion = update_encoder(info);
    if (motion == ENCODER_TURN_A)
    {
        input.direction = 1;
    }
    else if (motion == ENCODER_TURN_B)
    {
        input.direction = -1;
    }
    return input;
}

static void update_u_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    const encoder_info_t *info,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    const knob_input_t input = read_knob(info);
    if (!input.reset && input.direction == 0)
    {
        return;
    }

    if (input.held)
    {
        sensitivity /= 2;
    }
    sensitivity *= input.direction;

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input.reset)
    {
        *out = 0;
    }
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
    const encoder_info_t *info,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    const knob_input_t input = read_knob(info);
    if (!input.reset && input.direction == 0)
    {
        return;
    }

    if (input.held)
    {
        sensitivity /= 2;
    }
    sensitivity *= input.direction;

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input.reset)
    {
        *out = 0;
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

    update_u_value_from_encoder(vals_mutex, &(io->input_gain_knob.encoder), &(vals->input_gain), default_sensitivity);
    update_s_value_from_encoder(
        vals_mutex,
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

void init_knob_controls (uint8_t channel, channel_control_io_t *io)
{
    // TODO: per-channel config stuff (probably just choosing i2c bus based on channel arg)

    I2C_TypeDef *bus = i2c_channel_bus(channel);

    // INPUT GAIN KNOB CHANNEL 1
    button_info_t input_gain_button = {.bus = bus, .addr = 0x20, .port = MCP_GPIOB, .pin = 2, .active_low = true}; // dummy
    io->input_gain_knob.encoder = (encoder_info_t){.i2c_bus = bus,
                                                   .i2c_addr = 0x20,
                                                   .a_register = MCP_GPIOB,
                                                   .a_pin = 1,
                                                   .b_register = MCP_GPIOB,
                                                   .b_pin = 0,
                                                   .button_info = input_gain_button};

    // HF GAIN KNOB CHANNEL 1
    button_info_t hf_gain_button = {.bus = bus, .addr = 0x20, .port = MCP_GPIOB, .pin = 5, .active_low = true}; // dummy
    io->hf_interface.gain_knob.encoder = (encoder_info_t){.i2c_bus = bus,
                                                          .i2c_addr = 0x20,
                                                          .a_register = MCP_GPIOB,
                                                          .a_pin = 4,
                                                          .b_register = MCP_GPIOB,
                                                          .b_pin = 3,
                                                          .button_info = hf_gain_button};

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
