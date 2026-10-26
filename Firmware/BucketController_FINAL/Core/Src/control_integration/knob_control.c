#include "CONFIG.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
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

static void apply_u_input (
    SemaphoreHandle_t *out_mutex,
    const knob_input_t *input,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    if (!input->reset && input->direction == 0)
    {
        return;
    }
    sensitivity *= input->direction;

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input->reset)
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

static void apply_s_input (
    SemaphoreHandle_t *out_mutex,
    const knob_input_t *input,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity //!< sensitivity must be < S_SCALAR_CONTROL_MAX
)
{
    if (!input->reset && input->direction == 0)
    {
        return;
    }
    sensitivity *= input->direction;

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input->reset)
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

// Holding a knob's button makes adjustments finer
static inline s_scalar_control_t fine_if_held (const knob_input_t *input, s_scalar_control_t sensitivity)
{
    return input->held ? sensitivity / 2 : sensitivity;
}

static void update_u_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    const encoder_info_t *info,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity
)
{
    const knob_input_t input = read_knob(info);
    apply_u_input(out_mutex, &input, out, fine_if_held(&input, sensitivity));
}

static void update_s_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    const encoder_info_t *info,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity
)
{
    const knob_input_t input = read_knob(info);
    apply_s_input(out_mutex, &input, out, fine_if_held(&input, sensitivity));
}

/*
 * Each channel has 10 physical knobs: input gain (0), pan (9), and 8 shared knobs (1-8) whose function depends on the
 * selected control set. Each physical knob must only be read once per update, so only the knobs of the selected set are read.
 */

static void update_send_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io,
    s_scalar_control_t sensitivity
)
{
    for (uint8_t i = 0; i < SEND_CHANNELS; i++)
    {
        send_channel_interface_t *send_io = &(io->send_channel_interfaces[i]);
        send_channel_control_t *send_vals = &(vals->send_controls[i]);

        update_u_value_from_encoder(vals_mutex, &(send_io->gain_knob.encoder), &(send_vals->gain), sensitivity);
        if (i < SEND_CHANNELS - 1) // pan is not used for the last send channel
        {
            update_s_value_from_encoder(vals_mutex, &(send_io->pan_knob.encoder), &(send_vals->pan), sensitivity);
        }
    }
}

/**
 * @brief Update an EQ band's values. Bands with a q knob share one physical knob between gain and q:
 * turning it adjusts gain, or q while its button is held (so there is no fine adjustment on that knob).
 */
static void update_eq_band_knob_values (
    SemaphoreHandle_t *vals_mutex,
    eq_band_control_t *vals,
    eq_band_interface_t *band_io,
    bool has_q,
    s_scalar_control_t sensitivity
)
{
    if (has_q)
    {
        const knob_input_t input = read_knob(&(band_io->gain_knob.encoder)); // q_knob is the same hardware
        if (input.held)
        {
            apply_u_input(vals_mutex, &input, &(vals->q), sensitivity);
        }
        else
        {
            apply_s_input(vals_mutex, &input, &(vals->gain), sensitivity);
        }
    }
    else
    {
        update_s_value_from_encoder(vals_mutex, &(band_io->gain_knob.encoder), &(vals->gain), sensitivity);
    }

    update_u_value_from_encoder(vals_mutex, &(band_io->freq_knob.encoder), &(vals->freq), sensitivity);
}

static void update_comp_knob_values (
    SemaphoreHandle_t *vals_mutex,
    comp_control_t *vals,
    comp_interface_t *comp_io,
    s_scalar_control_t sensitivity
)
{
    update_s_value_from_encoder(vals_mutex, &(comp_io->in_gain_knob.encoder), &(vals->in_gain), sensitivity);
    update_s_value_from_encoder(vals_mutex, &(comp_io->threshold_knob.encoder), &(vals->threshold), sensitivity);
    update_u_value_from_encoder(vals_mutex, &(comp_io->attack_time_knob.encoder), &(vals->attack_time), sensitivity);
    update_u_value_from_encoder(vals_mutex, &(comp_io->release_time_knob.encoder), &(vals->release_time), sensitivity);
    update_u_value_from_encoder(vals_mutex, &(comp_io->ratio_knob.encoder), &(vals->ratio), sensitivity);
    update_u_value_from_encoder(vals_mutex, &(comp_io->out_gain_knob.encoder), &(vals->out_gain), sensitivity);
}

void update_channel_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    const s_scalar_control_t sensitivity = S_SCALAR_CONTROL_MAX / 32;

    // Always active, regardless of the selected control set
    update_u_value_from_encoder(vals_mutex, &(io->input_gain_knob.encoder), &(vals->input_gain), sensitivity);
    update_s_value_from_encoder(vals_mutex, &(io->pan.encoder), &(vals->output_pan), sensitivity);

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const channel_control_set_selection_t selection = vals->control_set_selection;
    xSemaphoreGive(*vals_mutex);

    switch (selection)
    {
        case SENDS_CONTROL:
            update_send_knob_values(vals_mutex, vals, io, sensitivity);
            break;
        case EQ_CONTROL:
            update_eq_band_knob_values(vals_mutex, &(vals->hf_control), &(io->hf_interface), false, sensitivity);
            update_eq_band_knob_values(vals_mutex, &(vals->hmf_control), &(io->hmf_interface), true, sensitivity);
            update_eq_band_knob_values(vals_mutex, &(vals->lmf_control), &(io->lmf_interface), true, sensitivity);
            update_eq_band_knob_values(vals_mutex, &(vals->lf_control), &(io->lf_interface), false, sensitivity);
            break;
        case COMP_CONTROL:
            update_comp_knob_values(vals_mutex, &(vals->comp_control), &(io->comp_interface), sensitivity);
            break;
    }
}


/*=============================== LEDS ================================*/

// The shared knobs' rings are registered as centered (signed), but some of their parameters are unsigned
static void show_unsigned (led_device_t *ring, u_scalar_control_t value)
{
    if (ring == NULL)
    {
        return;
    }
    if (ring->scale != SCALE_LEFT)
    {
        led_set_scale(ring, SCALE_LEFT);
    }
    led_set(ring, value);
}

static void show_signed (led_device_t *ring, s_scalar_control_t value)
{
    if (ring == NULL)
    {
        return;
    }
    if (ring->scale != SCALE_CENTER)
    {
        led_set_scale(ring, SCALE_CENTER);
    }
    led_set_signed(ring, value);
}

static void hide (led_device_t *ring) { show_unsigned(ring, 0); }

static void show_eq_band (const eq_band_control_t *vals, eq_band_interface_t *band_io, bool has_q)
{
    // Bands with a q knob show q on the shared ring while its button is held
    if (has_q && get_button_held(&(band_io->gain_knob.encoder.button_info)))
    {
        show_unsigned(band_io->q_knob.led_ring, vals->q);
    }
    else
    {
        show_signed(band_io->gain_knob.led_ring, vals->gain);
    }
    show_unsigned(band_io->freq_knob.led_ring, vals->freq);
}

void update_channel_knob_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const channel_controls v = *vals;
    xSemaphoreGive(*vals_mutex);

    show_unsigned(io->input_gain_knob.led_ring, v.input_gain);
    show_signed(io->pan.led_ring, v.output_pan);

    // Physical knobs 7 and 8 are the LF band's gain and freq knobs; the other sets leave them unused
    switch (v.control_set_selection)
    {
        case SENDS_CONTROL:
            for (uint8_t i = 0; i < SEND_CHANNELS; i++)
            {
                show_unsigned(io->send_channel_interfaces[i].gain_knob.led_ring, v.send_controls[i].gain);
                if (i < SEND_CHANNELS - 1) // pan is not used for the last send channel
                {
                    show_signed(io->send_channel_interfaces[i].pan_knob.led_ring, v.send_controls[i].pan);
                }
            }
            hide(io->lf_interface.freq_knob.led_ring);
            break;
        case EQ_CONTROL:
            show_eq_band(&(v.hf_control), &(io->hf_interface), false);
            show_eq_band(&(v.hmf_control), &(io->hmf_interface), true);
            show_eq_band(&(v.lmf_control), &(io->lmf_interface), true);
            show_eq_band(&(v.lf_control), &(io->lf_interface), false);
            break;
        case COMP_CONTROL:
            show_signed(io->comp_interface.in_gain_knob.led_ring, v.comp_control.in_gain);
            show_signed(io->comp_interface.threshold_knob.led_ring, v.comp_control.threshold);
            show_unsigned(io->comp_interface.attack_time_knob.led_ring, v.comp_control.attack_time);
            show_unsigned(io->comp_interface.release_time_knob.led_ring, v.comp_control.release_time);
            show_unsigned(io->comp_interface.ratio_knob.led_ring, v.comp_control.ratio);
            show_unsigned(io->comp_interface.out_gain_knob.led_ring, v.comp_control.out_gain);
            hide(io->lf_interface.gain_knob.led_ring);
            hide(io->lf_interface.freq_knob.led_ring);
            break;
    }
}


static const button_info_t KNOB_BUTTONS[10] = {
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

static const encoder_info_t KNOB_ENCODERS[10] = {
    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 0,
     .b_register = MCP_GPIOA,
     .b_pin = 0,
     .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 1,
     .b_register = MCP_GPIOA,
     .b_pin = 1,
     .button_info = KNOB_BUTTONS[1]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 2,
     .b_register = MCP_GPIOA,
     .b_pin = 2,
     .button_info = KNOB_BUTTONS[2]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 3,
     .b_register = MCP_GPIOA,
     .b_pin = 3,
     .button_info = KNOB_BUTTONS[3]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 4,
     .b_register = MCP_GPIOA,
     .b_pin = 4,
     .button_info = KNOB_BUTTONS[4]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 0,
     .b_register = MCP_GPIOA,
     .b_pin = 0,
     .button_info = KNOB_BUTTONS[5]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 1,
     .b_register = MCP_GPIOA,
     .b_pin = 1,
     .button_info = KNOB_BUTTONS[6]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 2,
     .b_register = MCP_GPIOA,
     .b_pin = 2,
     .button_info = KNOB_BUTTONS[7]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 3,
     .b_register = MCP_GPIOA,
     .b_pin = 3,
     .button_info = KNOB_BUTTONS[8]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 4,
     .b_register = MCP_GPIOA,
     .b_pin = 4,
     .button_info = KNOB_BUTTONS[9]}
};


// Index in led_driver.c's init_devices of the ring around each knob, by [channel][knob index in KNOB_ENCODERS].
// Knob 0 is input gain, 1-8 are the FX section rows 1A, 1B, 2A, 2B, 3A, 3B, 4A, 4B, and 9 is pan.
static const uint8_t KNOB_LED_RINGS[CHANNELS][10] = {
    {0, 39, 40, 47, 48, 55, 56, 95, 96, 103},
    {17, 38, 41, 46, 49, 54, 57, 78, 97, 102},
    {18, 37, 42, 45, 50, 53, 58, 77, 98, 101},
    {35, 36, 43, 44, 51, 52, 59, 60, 99, 100},
};

static void init_knob (uint8_t channel, knob_info_t *knob, uint8_t index)
{
    I2C_TypeDef *bus = i2c_channel_bus(channel);

    knob->encoder = KNOB_ENCODERS[index];
    knob->encoder.i2c_bus = bus;
    knob->encoder.button_info.bus = bus;
    knob->encoder.button_info.active_low = true;

    knob->led_ring = led_device_at(KNOB_LED_RINGS[channel][index]);
}


void init_knob_controls (uint8_t channel, channel_control_io_t *io)
{
    // INPUT AND PAN KNOB
    init_knob(channel, &(io->input_gain_knob), 0);
    init_knob(channel, &(io->pan), 9);

    // SENDS
    init_knob(channel, &(io->send_channel_interfaces[0].gain_knob), 1);
    init_knob(channel, &(io->send_channel_interfaces[0].pan_knob), 2);

    init_knob(channel, &(io->send_channel_interfaces[1].gain_knob), 3);
    init_knob(channel, &(io->send_channel_interfaces[1].pan_knob), 4);

    init_knob(channel, &(io->send_channel_interfaces[2].gain_knob), 5);
    init_knob(channel, &(io->send_channel_interfaces[2].pan_knob), 6);

    init_knob(channel, &(io->send_channel_interfaces[3].gain_knob), 7);

    // EQ HF
    init_knob(channel, &(io->hf_interface.gain_knob), 1);
    init_knob(channel, &(io->hf_interface.freq_knob), 2);

    // EQ HMF
    init_knob(channel, &(io->hmf_interface.gain_knob), 3);
    init_knob(channel, &(io->hmf_interface.freq_knob), 4);
    init_knob(channel, &(io->hmf_interface.q_knob), 3);

    // EQ LMF
    init_knob(channel, &(io->lmf_interface.gain_knob), 5);
    init_knob(channel, &(io->lmf_interface.freq_knob), 6);
    init_knob(channel, &(io->lmf_interface.q_knob), 5);

    // EQ LF
    init_knob(channel, &(io->lf_interface.gain_knob), 7);
    init_knob(channel, &(io->lf_interface.freq_knob), 8);

    // COMP
    init_knob(channel, &(io->comp_interface.in_gain_knob), 1);
    init_knob(channel, &(io->comp_interface.threshold_knob), 2);
    init_knob(channel, &(io->comp_interface.attack_time_knob), 3);
    init_knob(channel, &(io->comp_interface.release_time_knob), 4);
    init_knob(channel, &(io->comp_interface.ratio_knob), 5);
    init_knob(channel, &(io->comp_interface.out_gain_knob), 6);
}
