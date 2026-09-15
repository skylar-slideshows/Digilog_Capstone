#include <stdbool.h>
#include <stdint.h>
#include "CONFIG.h"

#include "control_integration/control_interface.h"

#include "button_control.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"
#include "rotary_encoder_control.h"


/*=============================== LOGIC CONNECTING HARDWARE TO CONTROL VALUES ================================*/

/*
 * Holds all control values for all channels in the bucket
 * defined in control_interface.h
 */
channel_controls channel_control_vals[CHANNELS];

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

/**
 * Updates control values based on current/previous (cached) input (e.g button/rotary-encoder) states
 * Gpio expanders should have been polled to cache before running this
 */
void update_control_values (void)
{
    for (uint8_t channel = 0; channel < CHANNELS; channel++)
    {
        update_channel_encoder_values(&(channel_control_vals[channel]), &(channel_states[channel]), &(channel_controls_io[4]));
        update_channel_button_values(&(channel_control_vals[channel]), &(channel_states[channel]), &(channel_controls_io[4]));

        // TODO: non-button/encoder inputs maybe?
    }
}

/*=============================== LOGIC CONNECTING CONTROL VALUES TO LEDS (TODO) ================================*/
