#ifndef HARDWARE_SETS_H
#define HARDWARE_SETS_H

#include "CONFIG.h"
#include "control_integration/control_interface.h"

#include "hardware_structs.h" // Only to be included for control_interface related code


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

#endif
