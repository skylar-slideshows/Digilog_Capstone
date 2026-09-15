#ifndef HARDWARE_STATE_SETS_H
#define HARDWARE_STATE_SETS_H

#include "hardware_drivers/rotary_encoder.h"
#include "control_integration/control_interface.h"

/**
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

/*=============================== HARDWARE STATE SETS FOR SECTIONS OF A CHANNEL ================================*/

typedef struct
{
    encoder_state_t gain_encoder_state;
    encoder_state_t q_encoder_state;
    encoder_state_t freq_encoder_state;
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

#endif
