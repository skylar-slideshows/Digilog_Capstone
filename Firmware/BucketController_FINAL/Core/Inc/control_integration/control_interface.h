#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "CONFIG.h"

#define SEND_CHANNELS 4

typedef uint16_t u_scalar_control_t;
typedef int16_t s_scalar_control_t;

#define U_SCALAR_CONTROL_MAX UINT16_MAX
#define U_SCALAR_CONTROL_MID U_SCALAR_CONTROL_MAX >> 1

#define S_SCALAR_CONTROL_MAX INT16_MAX
#define S_SCALAR_CONTROL_MIN INT16_MIN

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
    s_scalar_control_t gain; //!< -15 dB (INT16_MIN) to +15 dB (INT16_MAX), 0 = flat (cv_mapping.pdf 2-5)
    u_scalar_control_t q;    //!< LMF/HMF only: Q 0.5 (0, wide) to 5 (max, narrow), logarithmic (10-11)
    u_scalar_control_t freq; //!< logarithmic over the band: LF 65-550 Hz, LMF 180-2500 Hz, HMF 500-7000 Hz, HF 1.5-13 kHz (6-9)
    bool bell;               //!< LF/HF only: bell instead of shelf
} eq_band_control_t;

typedef struct
{
    u_scalar_control_t gain; //!< TODO desc / gain range
    s_scalar_control_t pan;  //!< TODO desc / pan range; note only applies to sends 1-3
} send_channel_control_t;

/** Compressor knobs, each 0-65535 from fully left (laws in comp/comp_engine.c), with their FX knob */
typedef struct
{
    u_scalar_control_t in_gain;      //!< K1 INPUT -10...+20 dB (0 dB a third round): how loud the detector sees the audio
    u_scalar_control_t out_gain;     //!< K5 OUTPUT -10...+20 dB (0 dB a third round): trim on the main VCAs after the fader
    u_scalar_control_t attack_time;  //!< K2 ATK 0.1...100 ms (log)
    u_scalar_control_t release_time; //!< K4 REL 10 ms...2 s (log)
    u_scalar_control_t threshold;    //!< K3 THRESH -30...+20 dBu
    u_scalar_control_t ratio;        //!< K6 RATIO 1:1...inf:1 (4:1 at the middle)
    u_scalar_control_t de_ess_amt;   //!< not used: the de-esser was dropped. Kept so the remote app's numbers don't shift
    bool rms;                        //!< detector: false = peak (startup), true = RMS. Hold THRESH 2 s to switch
} comp_control_t;


/**
 * @brief Full set of control values on a channel
 */
typedef struct
{
    /* OTHER CHANNEL STUFF */

    channel_input_selection_t input_type_selection;         //!< Mic/Line/Hi-Z input type
    channel_control_set_selection_t control_set_selection; //!< Which control set (sends/EQ/comp) the knobs currently control

    u_scalar_control_t input_gain;  //!< TODO desc / gain range (dB-like)
    s_scalar_control_t output_gain; //!< TODO desc / gain range (dB-like)
    s_scalar_control_t output_pan;  //!< TODO desc / pan range

    bool phantom_48v;      //!< TODO
    bool phase_flip;       //!< TODO
    bool high_pass_filter; //!< TODO

    bool muted; //!< Whether all output from this channel is muted
    bool solo;  //!< TODO
    bool sel;   //!< TODO

    bool ins;       //!< TODO
    bool pre_fader; //!< TODO
    bool record;    //!< TODO

    /* SENDS STUFF */

    send_channel_control_t send_controls[SEND_CHANNELS]; //!< Controls for send channels 1-4

    /* EQ STUFF */

    eq_band_control_t hf_control;  //!< EQ controls for the HF band
    eq_band_control_t hmf_control; //!< EQ controls for the HMF band
    eq_band_control_t lmf_control; //!< EQ controls for the LMF band
    eq_band_control_t lf_control;  //!< EQ controls for the LF band

    /* COMP STUFF */

    comp_control_t comp_control; //!< Compressor controls

    /* BYPASSES (double tap the EQ or SENDS button): the stored settings stay as they are */

    bool eq_off;    //!< EQ bypassed: every band flat, as if no gain were set
    bool sends_off; //!< all four sends at their lowest gain (off)
    bool comp_off;  //!< compressor bypassed: no gain reduction, and OUTPUT not applied (just fader and pan)
} channel_controls;


/**
 * @brief Holds all control values for all channels in the bucket
 */
extern channel_controls channel_control_vals[CHANNELS];

/**
 * @brief Mutexes matching channel_control_vals for each channel
 */
extern SemaphoreHandle_t channel_control_mutexes[CHANNELS];

/**
 * @brief Sets default channel control values and hardware configurations. Call before using control values.
 */
void init_control_interface (void);

/**
 * @brief Updates control values based on current/previous (cached) input (e.g button/rotary-encoder) states
 */
void update_control_values (uint8_t channel);

/**
 * @brief Updates LEDs / other visual outputs from stored control values
 */
void update_control_leds (uint8_t channel);

/**
 * @brief True while someone has a hand on a channel's fader, so a remote value mustn't move it
 * (fader_control.c; always false while FADERS_CONNECTED is 0)
 */
bool fader_control_touched (uint8_t channel);

/**
 * @brief Every control's value at startup. Knob resets (double click) and the EQ reset (hold the EQ button)
 * go back to these.
 */
const channel_controls *control_defaults (void);

/** Which knob value was changed last on a channel, for the screens' value pop-up */
typedef enum
{
    TOUCHED_NOTHING,
    TOUCHED_INPUT_GAIN,
    TOUCHED_PAN,
    TOUCHED_EQ_GAIN, //!< band says which: 0 LF, 1 LMF, 2 HMF, 3 HF
    TOUCHED_EQ_FREQ,
    TOUCHED_EQ_Q,
    TOUCHED_SEND_LEVEL, //!< band says which send: 0-3 (A1-A4)
    TOUCHED_SEND_PAN,   //!< band says which send: 0-1 (A1, A2: the stereo ones)
    TOUCHED_COMP_INPUT,
    TOUCHED_COMP_ATTACK,
    TOUCHED_COMP_THRESH,
    TOUCHED_COMP_RELEASE,
    TOUCHED_COMP_OUTPUT,
    TOUCHED_COMP_RATIO,
    TOUCHED_COMP_MODE,  //!< peak/RMS switched (hold THRESH)
} touched_param_t;

typedef struct
{
    touched_param_t param;
    uint8_t band;     //!< EQ band for the EQ values (0 LF, 1 LMF, 2 HMF, 3 HF), or the send (0-3)
    uint32_t tick;    //!< FreeRTOS tick (ms) of the last change
} touched_t;

/**
 * @brief The value most recently changed by a knob on a channel (turned or reset), and when.
 * param is TOUCHED_NOTHING until a knob has changed something.
 */
touched_t control_last_touched (uint8_t channel);

#endif
