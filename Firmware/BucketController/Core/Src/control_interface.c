#include <stdbool.h>
#include <stdint.h>

#include "CONFIG.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/button_driver.h"
#include "stm32g474xx.h"
#include "control_interface.h"


/*=============================== INDIVIDUAL HARDWARE DATA STRUCTURES ================================*/


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

/**
 * @brief Info on the rotary encoder and led ring of one knob
 */
typedef struct
{
    encoder_info_t encoder;
    led_ring_info_t led_ring;
} knob_info_t;

/**
 * @brief Info on the button and led of a lit button
 */
typedef struct
{
    button_info_t button;
    button_led_info_t led;
} lit_button_info_t;

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


/*=============================== HARDWARE SETS FOR SECTIONS OF A CHANNEL ================================*/


/**
 * @brief Info on physical interfaces related to a single EQ band on a single channel
 */
typedef struct
{
    knob_info_t gain_knob;
    knob_info_t q_knob;
    knob_info_t freq_knob;
} eq_band_interface_t;


/**
 * @brief Info on physical interfaces related to a single send channel output from a single console channel
 */
typedef struct
{
    knob_info_t gain_knob;
    knob_info_t pan_knob; //!< Note that pan is not used for send channel 4
} send_channel_interface_t;

typedef struct
{
    knob_info_t in_gain_knob;
    knob_info_t out_gain_knob;
    knob_info_t attack_time_knob;
    knob_info_t release_time_knob;
    knob_info_t threshold_knob;
    knob_info_t ratio_knob;
    knob_info_t de_ess_amt_knob;
} comp_interface_t;


typedef struct
{
    lit_button_info_t mic_input_button;
    lit_button_info_t line_input_button;
    lit_button_info_t hiz_input_button;
    knob_info_t input_gain_knob;
    lit_button_info_t phantom_48v_button;
    lit_button_info_t phase_flip_button;
    lit_button_info_t high_pass_filter_button;
    lit_button_info_t send_button;
    lit_button_info_t eq_button;
    lit_button_info_t comp_button;

    send_channel_interface_t send_channel_interfaces[SEND_CHANNELS];

    eq_band_interface_t hf_interface;
    eq_band_interface_t hmf_interface;
    eq_band_interface_t lmf_interface;
    eq_band_interface_t lf_interface;

    comp_interface_t comp_interface;

    lit_button_info_t solo_button;
    lit_button_info_t mute_button;
    lit_button_info_t sel_button;

    lit_button_info_t ins_button;
    lit_button_info_t pre_button;
    lit_button_info_t rec_button;

    fader_info_t fader;
    fader_led_bar_info_t fader_led_bar;
} channel_control_io_t;


/*=============================== HARDWARE STATE SETS FOR SECTIONS OF A CHANNEL ================================*/


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


/*=============================== LOGIC CONNECTING HARDWARE TO CONTROL VALUES ================================*/


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
        &(channel_controls_io->input_gain_knob.encoder),
        &(vals->input_gain),
        get_button_state(&channel_controls_io->input_gain_knob.encoder.button_info) ? default_sensitivity / 2 : default_sensitivity
    );

    // TODO: Do this for the rest of the encoders on the channel
}

static void update_channel_button_values (uint8_t channel)
{
    channel_controls *vals = &(channel_control_vals[channel]);
    channel_control_io_state *state = &(channel_states[channel]);

    update_toggle_button_val_from_info(&(state->mute_button_state), &(channel_controls_io->mute_button.button), &(vals->muted));

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

/*=============================== LOGIC CONNECTING CONTROL VALUES TO LEDS (TODO) ================================*/
