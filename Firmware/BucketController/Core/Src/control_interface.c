#include <stdbool.h>
#include <stdint.h>

#include "CONFIG.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/button_driver.h"
#include "stm32g474xx.h"
#include "control_interface.h"


/**
 * @brief Info on the location of a single button LED
 */
typedef struct
{
    // TODO
} button_led_info_t;

/**
 * @brief Info on the location of a single LED ring
 */
typedef struct
{
    uint8_t channel;
    uint8_t knob_num;
} led_ring_info_t;

//! PLACEHOLDER for when the fader driver is created
typedef struct
{
    // TODO / PLACEHOLDER
} fader_info_t;

//! PLACEHOLDER for when the fader driver is created
typedef struct
{
    // TODO / PLACEHOLDER
} fader_led_bar_info_t;


/**
 * @brief Info on physical interfaces related to a single EQ band on a single channel
 */
typedef struct
{
    encoder_info_t gain_encoder;
    led_ring_info_t gain_led_ring;

    encoder_info_t q_encoder;
    led_ring_info_t q_led_ring;

    encoder_info_t freq_encoder;
    led_ring_info_t freq_led_ring;
} eq_band_interface_t;


/**
 * @brief Info on physical interfaces related to a single send channel output from a single console channel
 */
typedef struct
{
    encoder_info_t gain_encoder;
    led_ring_info_t gain_led_ring;

    encoder_info_t pan_encoder;   //!< Note that pan is not used for send channel 4
    led_ring_info_t pan_led_ring; //!< Note that pan is not used for send channel 4
} send_channel_interface_t;

typedef struct
{
    encoder_info_t in_gain_encoder;
    led_ring_info_t in_gain_led_ring;

    encoder_info_t out_gain_encoder;
    led_ring_info_t out_gain_led_ring;


    encoder_info_t attack_time_encoder;
    led_ring_info_t attack_time_led_ring;

    encoder_info_t release_time_encoder;
    led_ring_info_t release_time_led_ring;

    encoder_info_t threshold_encoder;
    led_ring_info_t threshold_led_ring;

    encoder_info_t ratio_encoder;
    led_ring_info_t ratio_led_ring;

    encoder_info_t de_ess_amt_encoder;
    led_ring_info_t de_ess_amt_led_ring;
} comp_interface_t;


typedef struct
{
    button_info_t mic_input_button;
    button_led_info_t mic_input_led;

    button_info_t line_input_button;
    button_led_info_t line_input_led;

    button_info_t hiz_input_button;
    button_led_info_t hiz_input_led;


    encoder_info_t input_gain_encoder;
    led_ring_info_t input_gain_led_ring;


    button_info_t phantom_48v_button;
    button_led_info_t phantom_48v_led;

    button_info_t phase_flip_button;
    button_led_info_t phase_flip_led;

    button_info_t high_pass_filter_button;
    button_led_info_t high_pass_filter_led;


    button_info_t send_button;
    button_led_info_t send_led;

    button_info_t eq_button;
    button_led_info_t eq_led;

    button_info_t comp_button;
    button_led_info_t comp_led;


    send_channel_interface_t send_channel_interfaces[SEND_CHANNELS];


    eq_band_interface_t hf_interface;
    eq_band_interface_t hmf_interface;
    eq_band_interface_t lmf_interface;
    eq_band_interface_t lf_interface;

    comp_interface_t comp_interface;


    button_info_t solo_button;
    button_led_info_t solo_led;

    button_info_t mute_button;
    button_led_info_t mute_led;

    button_info_t sel_button;
    button_led_info_t sel_led;


    button_info_t ins_button;
    button_led_info_t ins_led;

    button_info_t pre_button;
    button_led_info_t pre_led;

    button_info_t rec_button;
    button_led_info_t rec_led;

    fader_info_t fader;
    fader_led_bar_info_t fader_led_bar;
} channel_control_io_t;


typedef struct
{
    encoder_turn_action_t gain_encoder_state;
    encoder_turn_action_t q_encoder_state;
    encoder_turn_action_t freq_encoder_state;
} eq_band_interface_state_t;

typedef struct
{
    encoder_state_t gain_encoder;
    encoder_state_t pan_encoder; //!< Note that pan is not used for send channel 4
} send_channel_interface_state_t;

typedef struct
{
    encoder_state_t in_gain_encoder_state;
    encoder_state_t out_gain_encoder_state;
    encoder_state_t attack_time_encoder_state;
    encoder_state_t release_time_encoder_state;
    encoder_state_t threshold_encoder_state;
    encoder_state_t ratio_encoder_state;
    encoder_state_t de_ess_amt_encoder_state;
} comp_interface_state_t;

/**
 * @brief struct containing a snapshot of input states on one channel
 */
typedef struct
{
    bool mic_input_button_state;
    bool line_input_button_state;
    bool hiz_input_button_state;

    encoder_state_t input_gain_encoder_state;

    bool phantom_48v_button_state;
    bool phase_flip_button_state;
    bool high_pass_filter_button_state;

    bool send_button_state;
    bool eq_button_state;
    bool comp_button_state;

    send_channel_interface_state_t send_channel_interface_states[SEND_CHANNELS];

    eq_band_interface_state_t hf_interface_state;
    eq_band_interface_state_t hmf_interface_state;
    eq_band_interface_state_t lmf_interface_state;
    eq_band_interface_state_t lf_interface_state;

    comp_interface_state_t comp_interface_state;

    bool solo_button_state;
    bool mute_button_state;
    bool sel_button_state;

    bool ins_button_state;
    bool pre_button_state;
    bool rec_button_state;

    // TODO: Hold duration counters for buttons
} channel_control_io_state;


/* ACTUAL CODE STARTS BELOW no more data structures PLEASE, i have a headache and this is way too many lines for one C file */


/**
 * @brief Holds info on all control interface IO for all 4 channels in the bucket
 */
channel_control_io_t channel_controls_io[CHANNELS];
channel_control_io_state channel_states[CHANNELS];

static void init_control_io (uint8_t channel)
{
    // TODO: Fill channel_controls_io to match the hardware
}

static void init_control_vals (uint8_t channel)
{
    channel_controls *vals = &(channel_control_vals[channel]);

    vals->input_type_selection = LINE_INPUT;
    vals->input_gain = 0;
    vals->output_gain = 0;
    vals->output_pan = 0;

    vals->phantom_48v = false;
    vals->phase_flip = false;
    vals->high_pass_filter = false;

    vals->muted = false;

    vals->ins = false;
    vals->pre_fader = false;
    vals->record = false;

    for (uint8_t send_channel = 0; send_channel < SEND_CHANNELS; send_channel++)
    {
        vals->send_controls[send_channel].gain = 0;
        vals->send_controls[send_channel].pan = 0;
    }

    const eq_band_control_t default_eq_band_control = {.freq = 0, .gain = 0, .q = U_SCALAR_CONTROL_MID};
    vals->hf_control = default_eq_band_control;
    vals->hmf_control = default_eq_band_control;
    vals->lmf_control = default_eq_band_control;
    vals->lf_control = default_eq_band_control;

    vals->comp_control.attack_time = U_SCALAR_CONTROL_MID;
    vals->comp_control.release_time = U_SCALAR_CONTROL_MID;
    vals->comp_control.in_gain = 0;
    vals->comp_control.out_gain = 0;
    vals->comp_control.threshold = 0;
    vals->comp_control.de_ess_amt = 0;
    vals->comp_control.ratio = 0;
}

void init_control_interface (void)
{
    for (uint8_t channel = 0; channel < CHANNELS; channel++)
    {
        init_control_io(channel);
        init_control_vals(channel);
    }
}


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

static void update_toggle_button_val_from_info (bool *btn_state, button_info_t *info, bool *val_out)
{
    bool new_button_state = get_button_state(info);

    if (new_button_state && !*btn_state)
    {
        *val_out = !*val_out;
    }

    *btn_state = new_button_state;
}

static void update_channel_encoder_values (uint8_t channel)
{
    channel_controls *vals = &(channel_control_vals[channel]);
    channel_control_io_state *state = &(channel_states[channel]);

    s_scalar_control_t default_sensitivity = S_SCALAR_CONTROL_MAX / 16;

    update_s_value_from_encoder_motion(
        &(state->input_gain_encoder_state),
        &(channel_controls_io->input_gain_encoder),
        &(vals->input_gain),
        get_button_state(&channel_controls_io->input_gain_encoder.button_info) ? default_sensitivity / 2 : default_sensitivity
    );

    // TODO: Do this for the rest of the encoders on the channel
}

static void update_channel_button_values (uint8_t channel)
{
    channel_controls *vals = &(channel_control_vals[channel]);
    channel_control_io_state *state = &(channel_states[channel]);

    update_toggle_button_val_from_info(&(state->mute_button_state), &(channel_controls_io->mute_button), &(vals->muted));

    // TODO: Do this for the rest of the toggle-buttons on the channel
    // TODO: handle non-toggle (e.g radio) buttons
}

/**
 * Updates control values based on current/previous (cached) input (e.g button/rotary-encoder) states
 * Gpio expanders should have been polled to cache before running this
 */
void update_control_values (void)
{
    for (uint8_t channel = 0; channel < CHANNELS; channel++)
    {
        update_channel_button_values(channel);
        update_channel_encoder_values(channel);

        // TODO: non-button/encoder inputs maybe?
    }
}
