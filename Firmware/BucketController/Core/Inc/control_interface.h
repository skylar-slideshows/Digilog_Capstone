#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/led_driver.h"

typedef uint32_t u_scalar_control_t;
typedef int32_t s_scalar_control_t;

typedef enum
{
    MIC_INPUT,
    LINE_INPUT,
    HIZ_INPUT,
} channel_input_selection_t;

typedef enum
{
    SENDS_CONTROL,
    EQ_CONTROL,
    COMP_CONTROL,
} channel_control_set_selection_t;


typedef struct
{
    s_scalar_control_t gain; //!< TODO gain range
    u_scalar_control_t q;    //!< TODO q range(s)
    u_scalar_control_t freq; //!< TODO freq range(s)
} eq_band_control_t;

typedef struct
{
    s_scalar_control_t gain; //!< TODO desc / gain range
    s_scalar_control_t pan;  //!< TODO desc / pan range; note only applies to sends 1-3
} send_channel_control_t;

typedef struct
{
    s_scalar_control_t in_gain;  //!< TODO desc / gain range (dB-like)
    u_scalar_control_t out_gain; //!< TODO desc / gain range (dB-like)

    u_scalar_control_t attack_time;  //!< Time before gain is reduced after vol>threshold / attack range (time; 100us-100ms ish)
    u_scalar_control_t release_time; //!< TODO desc / release range (time; 10ms-5sec ish)
    s_scalar_control_t threshold;    //!< Max volume of input / TODO thresh range (dB-like)
    u_scalar_control_t ratio;        //!< TODO desc / ratio range (unitless on log scale; 0 = 1, MAX/2 = 4, MAX=~inf or 40)
    u_scalar_control_t de_ess_amt;   //!< TODO desc / De-Ess Amt range (dB-like; same units as threshold)
} comp_control_t;

typedef struct
{
    channel_input_selection_t input_type_selection;

    s_scalar_control_t input_gain;

    bool phantom_48v;
    bool phase_flip;
    bool high_pass_filter;

    /* SENDS STUFF */

    send_channel_control_t send_controls[4];

    /* EQ STUFF */

    eq_band_control_t hf_control;
    eq_band_control_t hmf_control;
    eq_band_control_t lmf_control;
    eq_band_control_t lf_control;

    /* COMP STUFF */

    comp_control_t comp_control;
} channel_controls;

#endif
