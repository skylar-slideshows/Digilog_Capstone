/**
  **********************************************************************************
  * CV MAPPING - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file cv_mapping.h
  * @brief Turns a channel's stored 16-bit control values into the 12-bit MCP4728 codes
  *        (0x000-0xFFF = 0-2.0475 V, 0.5 mV a step) and the routing board's switch bits,
  *        following "CV mapping: parameter value P (0-65535) -> DAC code (0-4095)"
  *        (cv_mapping.pdf). Plain C with no hardware calls, so it can be checked on a PC.
  *
  * Knob position x = P/65535 (0 = fully left). Centred knob c = (P-32767)/32767 for
  * P <= 32767, else (P-32767)/32768 (-1 ... +1). Signed control values (int16) are
  * turned into P by flipping the top bit (-32768 -> 0, 0 -> 32768, 32767 -> 65535).
  *
  * The volts-per-dB constants in cv_mapping.c reproduce every code in the PDF's tables.
  **********************************************************************************
*/

#ifndef CV_MAPPING_H
#define CV_MAPPING_H

#include <stdbool.h>
#include <stdint.h>

#define CV_CODE_MAX 4095U // 0xFFF = 2.0475 V

/** Every CV a channel has. Each lands on one MCP4728 output, the same on all four strips. */
typedef enum
{
    CV_INPUT_GAIN, //!< 0x60 C
    CV_LF_GAIN,    //!< 0x61 C
    CV_LMF_GAIN,   //!< 0x60 B
    CV_HMF_GAIN,   //!< 0x62 D
    CV_HF_GAIN,    //!< 0x61 A
    CV_LF_FREQ,    //!< 0x61 B
    CV_LMF_FREQ,   //!< 0x60 A
    CV_HMF_FREQ,   //!< 0x62 C
    CV_HF_FREQ,    //!< 0x63 B
    CV_LMF_Q,      //!< 0x61 D
    CV_HMF_Q,      //!< 0x60 D
    CV_AUX1_L,     //!< 0x62 A
    CV_AUX1_R,     //!< 0x62 B
    CV_AUX2_L,     //!< 0x64 D
    CV_AUX2_R,     //!< 0x64 A
    CV_AUX3,       //!< 0x64 C (drives both send-3 VCAs)
    CV_AUX4,       //!< 0x64 B
    CV_OUT_L,      //!< 0x63 D
    CV_OUT_R,      //!< 0x63 C
    CV_COUNT
} cv_id_t;

/** Where a CV goes: MCP4728 I2C address and output (0 = A ... 3 = D). 0x63 A is not used here. */
typedef struct
{
    uint8_t addr;
    uint8_t output;
} cv_dac_slot_t;

extern const cv_dac_slot_t CV_DAC_SLOTS[CV_COUNT];

/** Input source, as selected on the strip */
typedef enum
{
    CV_SOURCE_MIC,
    CV_SOURCE_LINE,
    CV_SOURCE_HIZ,
} cv_source_t;

/** One EQ band's settings, all as P (0-65535). gain is centred (P 32767/32768 = 0 dB). */
typedef struct
{
    uint16_t gain;
    uint16_t freq;
    uint16_t q;    //!< LMF and HMF only
    bool bell;     //!< LF and HF only: bell instead of shelf
} cv_eq_band_t;

/** Everything one channel's CVs and switches depend on */
typedef struct
{
    cv_source_t source;
    uint16_t input_gain;          //!< P
    cv_eq_band_t lf, lmf, hmf, hf;
    uint16_t aux_level[4];        //!< P, sends 1-4
    uint16_t aux_pan[2];          //!< P (centred), sends 1 and 2 only
    uint16_t fader;               //!< P; 0 mutes the channel
    uint16_t pan;                 //!< P (centred), main pan
    bool phantom, phase, hpf;
    bool insert, insert_pre;      //!< insert_pre: the insert send taps before the EQ
    bool muted;                   //!< the user's mute: off the mix bus (sends keep going)
    bool soloed;                  //!< this channel is soloed
    bool other_soloed;            //!< another channel in the bucket is soloed: off the mix bus unless soloed too
} cv_params_t;

/** Remembered between updates */
typedef struct
{
    bool pad_on; //!< mic pad, switched with hysteresis
} cv_state_t;

/** What to send */
typedef struct
{
    uint16_t code[CV_COUNT]; //!< MCP4728 codes, 0-4095
    uint16_t switches;       //!< routing board bits for this channel (RS_* in routing_switches.h)
    float input_total_db;    //!< T: total input gain, for display
    float comp_extra_db;     //!< E: attenuation (<= 0 dB) the output VCAs can't reach, for the compressor CV
} cv_result_t;

/** Works out every code and switch bit for one channel */
void cv_compute (const cv_params_t *params, cv_state_t *state, cv_result_t *out);

/* Building blocks, in the PDF's terms */
float cv_knob (uint16_t p);                                     //!< x = P/65535
float cv_centred (uint16_t p);                                  //!< c, -1 ... +1
uint16_t cv_signed_to_p (int16_t value);                        //!< int16 control value -> P
float cv_fader_db (uint16_t p);                                 //!< F; -INFINITY when P = 0
void cv_pan_db (uint16_t p, float *left_db, float *right_db);   //!< constant-power pan law
uint16_t cv_eq_gain_code (uint16_t p, bool *cut);               //!< and whether the band must be wired to cut

#endif
