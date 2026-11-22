/**
  **********************************************************************************
  * COMPRESSOR ENGINE - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file comp/comp_engine.h
  * @brief The digital half of the hybrid compressor, with no hardware in it (so it can be tested
  *        on a PC). The comp card turns the channel's audio into an envelope (ENV_ADC = 0.221 x
  *        |audio|); this reads that envelope one sample at a time, works out the gain reduction
  *        from the knobs (input, threshold, ratio, attack, release, output), and gives the DAC
  *        voltage that the comp card's scaler turns into the VCAs' second control voltage
  *        (COMP_CV, to the fader/VCA card's E- through JP1).
  *
  * Signal flow per sample:
  *   level  = 20 log10(env / 0.2421) dBu + INPUT           comp card note: 0 dBu sine = 0.2421 V peak.
  *                                                         INPUT only makes the detector see it louder.
  *   GR     = gain computer(level, THRESH, RATIO)          >= 0 dB, hard knee
  *   GR'    = GR smoothed: ATK while it rises, REL while it falls
  *   gain   = E - GR', limited to -40...+20 dB              the scaler's range
  *   V(DAC) = (0.2685 + 0.0061 x gain) / k, k = 10k / R7    comp card note (R7 48.7k: k = 0.2053)
  *
  * OUTPUT isn't here: it trims the main L/R VCA CVs after the fader and pan (cv_mapping.c). Both CVs
  * drive the same VCAs, so E is whatever part of fader + pan + OUTPUT the main CVs can't reach
  * (above about +11.8 dB, or below -40 dB); the compressor CV carries it instead.
  **********************************************************************************
*/

#ifndef COMP_ENGINE_H
#define COMP_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

/* The knob ranges printed on the faceplate */
#define COMP_INPUT_MIN_DB    -10.0f  // INPUT: -10 ... +20 dB (0 dB a third of the way round)
#define COMP_INPUT_MAX_DB     20.0f
#define COMP_OUTPUT_MIN_DB   -10.0f  // OUTPUT: -10 ... +20 dB (0 dB a third of the way round)
#define COMP_OUTPUT_MAX_DB    20.0f
#define COMP_THRESH_MIN_DBU  -30.0f  // THRESH: -30 ... +20 dBu
#define COMP_THRESH_MAX_DBU   20.0f
#define COMP_ATTACK_MIN_MS     0.1f  // ATK: 0.1 ... 100 ms, log taper
#define COMP_ATTACK_MAX_MS   100.0f
#define COMP_RELEASE_MIN_MS   10.0f  // REL: 10 ms ... 2 s, log taper
#define COMP_RELEASE_MAX_MS 2000.0f
                                     // RATIO: 1:1 ... inf:1 (4:1 at the middle)

/* The comp card's scaler (its schematic notes) and what the VCAs can do through it */
#define COMP_GAIN_MIN_DB     -40.0f  // deepest gain reduction the scaler is designed for
#define COMP_GAIN_MAX_DB      20.0f  // most make-up gain
#define COMP_SCALER_OFFSET_V  0.2685f
#define COMP_SCALER_V_PER_DB  0.0061f
#define COMP_ENV_0DBU_V       0.2421f // envelope voltage of a 0 dBu sine's peak
#define COMP_RMS_MS           10.0f   // RMS detector: averaging time constant

/** The compressor knobs the engine uses, as stored (each 0-65535, the knob's position from fully left).
 *  OUTPUT isn't one of them: see above. */
typedef struct
{
    uint16_t input;     //!< INPUT
    uint16_t attack;    //!< ATK
    uint16_t threshold; //!< THRESH
    uint16_t release;   //!< REL
    uint16_t ratio;     //!< RATIO
    bool rms;           //!< detector: false = peak, true = RMS
    bool bypass;        //!< no gain reduction applied (still worked out, for the meter)
} comp_knobs_t;

/** Everything the per-sample step needs, worked out from the knobs once when they change */
typedef struct
{
    float input_db;      //!< INPUT: added to the level the gain computer sees
    float threshold_dbu;
    float slope;         //!< 1 - 1/ratio: 0 = 1:1 (no compression) ... 1 = inf:1
    float attack_coeff;  //!< one-pole coefficient per sample while the gain reduction rises
    float release_coeff; //!< ... while it falls
    float rms_coeff;     //!< the RMS detector's averaging
    bool rms;
    bool bypass;
} comp_coeffs_t;

/** One channel's running state */
typedef struct
{
    float gr_db;   //!< smoothed gain reduction, dB (>= 0)
    float peak_gr; //!< peak detector: each peak's gain reduction, held and falling at the release rate
    float ms;      //!< mean square of the envelope (RMS detector; kept up to date in peak mode too)
} comp_state_t;

/* Knob position (0-65535) to value, following the faceplate */
float comp_input_db (uint16_t p);
float comp_output_db (uint16_t p);     //!< OUTPUT's trim, used by cv_mapping for the main VCA CVs
float comp_threshold_dbu (uint16_t p);
float comp_attack_ms (uint16_t p);
float comp_release_ms (uint16_t p);
float comp_ratio (uint16_t p);          //!< 1 ... (large); returns INFINITY fully right
float comp_slope (uint16_t p);          //!< 1 - 1/ratio, 0 ... 1

/** Knob position that gives a value, for defaults */
uint16_t comp_p_from_fraction (float x);

/** Works out the coefficients from the knobs; sample_hz is the rate comp_engine_step is called at */
void comp_engine_coeffs (comp_coeffs_t *c, const comp_knobs_t *k, float sample_hz);

/** Static curve: gain reduction (dB, >= 0) for a level, before smoothing */
float comp_engine_target_gr (const comp_coeffs_t *c, float level_dbu);

/**
 * One sample: envelope voltage in, gain for the compressor CV out (dB: extra_db - gain reduction,
 * limited to the scaler's -40...+20 dB; no gain reduction while bypassed). extra_db is E from
 * cv_mapping. s->gr_db holds the gain reduction, for metering.
 *
 * Peak: the level is the rectified envelope itself. Each peak's gain reduction is caught at once and
 * held, falling back at the release rate between peaks, and the attack then smooths how fast the
 * gain comes down (a "decoupled" peak detector, so a steady sine reads its peak). RMS: the level is
 * the envelope's mean square over COMP_RMS_MS, scaled so a steady sine reads the same as in peak
 * mode; the attack and release work on its gain reduction directly.
 */
float comp_engine_step (const comp_coeffs_t *c, comp_state_t *s, float env_volts, float extra_db);

/**
 * DAC output for a gain: volts from the scaler's law, then the 12-bit code for a DAC whose full
 * scale is dac_vref volts. scaler_k is 10k / R7 (0.2053 for R7 = 48.7k).
 */
float comp_engine_dac_volts (float gain_db, float scaler_k);
uint16_t comp_engine_dac_code (float gain_db, float scaler_k, float dac_vref);

/** Fast log2 (worst error 1.4e-5) for the per-sample maths */
float comp_fast_log2 (float x);

#endif
