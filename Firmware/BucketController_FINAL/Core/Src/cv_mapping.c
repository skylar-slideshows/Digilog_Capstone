/**
  **********************************************************************************
  * CV MAPPING - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file cv_mapping.c
  * @brief See cv_mapping.h. Section numbers refer to cv_mapping.pdf.
  **********************************************************************************
*/

#include "cv_mapping.h"
#include "hardware_drivers/routing_switches.h"
#include <math.h>

#define VOLTS_PER_CODE 0.0005f

/* For now the signal path is fixed straight through: no insert, the insert send taken after the EQ
   (not "pre"), and +48 V always off. Set these to 1 to let the buttons control them. */
#define CV_ENABLE_INSERT  0
#define CV_ENABLE_PHANTOM 0

const cv_dac_slot_t CV_DAC_SLOTS[CV_COUNT] = {
    [CV_INPUT_GAIN] = {0x60, 2},
    [CV_LF_GAIN] = {0x61, 2},
    [CV_LMF_GAIN] = {0x60, 1},
    [CV_HMF_GAIN] = {0x62, 3},
    [CV_HF_GAIN] = {0x61, 0},
    [CV_LF_FREQ] = {0x61, 1},
    [CV_LMF_FREQ] = {0x60, 0},
    [CV_HMF_FREQ] = {0x62, 2},
    [CV_HF_FREQ] = {0x63, 1},
    [CV_LMF_Q] = {0x61, 3},
    [CV_HMF_Q] = {0x60, 3},
    [CV_AUX1_L] = {0x62, 0},
    [CV_AUX1_R] = {0x62, 1},
    [CV_AUX2_L] = {0x64, 3},
    [CV_AUX2_R] = {0x64, 0},
    [CV_AUX3] = {0x64, 2},
    [CV_AUX4] = {0x64, 1},
    [CV_OUT_L] = {0x63, 3},
    [CV_OUT_R] = {0x63, 2},
};

/*=============================== VCA LAWS ================================*/
/* Each VCA's control voltage is a straight line in dB. These constants reproduce every code in
   the PDF's tables (fitted to the tables, with the most room to spare before any code would change). */

#define INPUT_V0     1.5115921f   // section 1: VCA gain G = T - F, volts at G = 0 dB
#define INPUT_V_DB   0.01984211f  //            volts per dB
#define EQ_V0        1.7058067f   // sections 2-5: band VCA gain a, volts at a = 1 (0 dB)
#define EQ_V_DB      0.02283869f
#define AUX_V0       1.8247911f   // sections 12-17: send level A, volts at 0 dB
#define AUX_V_DB     0.02204240f
#define OUT_V0       0.4655629f   // sections 18-19: output gain G, volts at 0 dB; more volts = quieter
#define OUT_V_DB     -0.03940625f
#define OUT_FLOOR_DB -40.16f      // the output VCAs' deepest cut (at 0xFFF); beyond it goes to the compressor

static uint16_t volts_to_code (float volts)
{
    const float code = volts / VOLTS_PER_CODE;
    if (!(code > 0.0f)) return 0; // also catches NaN and -inf
    if (code >= (float)CV_CODE_MAX) return CV_CODE_MAX;
    return (uint16_t)lroundf(code);
}

static uint16_t line_code (float code)
{
    if (code <= 0.0f) return 0;
    if (code >= (float)CV_CODE_MAX) return CV_CODE_MAX;
    return (uint16_t)lroundf(code);
}

/** A straight line in knob position: code0 at x = 0, code1 at x = 1 (sections 6-11) */
static uint16_t knob_line (uint16_t p, float code0, float code1)
{
    return line_code(code0 + (code1 - code0) * cv_knob(p));
}

/*=============================== KNOBS ================================*/

float cv_knob (uint16_t p) { return (float)p / 65535.0f; }

float cv_centred (uint16_t p)
{
    const float d = (float)p - 32767.0f;
    return p <= 32767U ? d / 32767.0f : d / 32768.0f;
}

uint16_t cv_signed_to_p (int16_t value) { return (uint16_t)((uint16_t)value ^ 0x8000U); }

/*=============================== 1. INPUT GAIN ================================*/

static const float INPUT_RANGE[3][2] = {
    [CV_SOURCE_MIC] = {10.0f, 65.0f},
    [CV_SOURCE_LINE] = {-36.0f, 21.0f},
    [CV_SOURCE_HIZ] = {-24.0f, 33.0f},
};

float cv_input_gain_db (cv_source_t source, uint16_t p)
{
    if (source > CV_SOURCE_HIZ) source = CV_SOURCE_LINE;
    return INPUT_RANGE[source][0] + cv_knob(p) * (INPUT_RANGE[source][1] - INPUT_RANGE[source][0]);
}

static uint16_t input_gain_code (cv_source_t source, uint16_t p, bool *pad_on, float *total_db)
{
    const float t = cv_input_gain_db(source, p);
    float fixed;
    if (source == CV_SOURCE_MIC)
    {
        if (t < 38.0f) *pad_on = true; // hysteresis: between 38 and 42 dB the pad stays as it was
        else if (t > 42.0f) *pad_on = false;
        fixed = *pad_on ? 20.0f : 40.0f;
    }
    else
    {
        fixed = (source == CV_SOURCE_LINE) ? -6.0f : 6.0f;
    }
    *total_db = t;
    return volts_to_code(INPUT_V0 + INPUT_V_DB * (t - fixed));
}

/*=============================== 2-5. EQ GAIN ================================*/
/*
 * The gain knobs are "zoomed in" around 0 dB instead of the PDF's straight B = 15 c:
 *     B = 15 (a c + (1 - a) c^3) dB        c = centred knob, -1 ... +1
 * Still -15 dB fully left, 0 dB in the middle, +15 dB fully right, but the knob travel for 0 to +1 dB
 * is 1.5 times the travel for +14 to +15 dB on a bell, 1.8 times on a shelf (exactly, not just the
 * slope). The stored value and the LED ring are unchanged, so the ring still moves evenly per step of
 * the knob; only the dB per step changes.
 */
#define EQ_TAPER_BELL  0.8501585f // LMF, HMF, and LF/HF switched to bell: 1.5x
#define EQ_TAPER_SHELF 0.7806492f // LF/HF as shelves: 1.8x

float cv_eq_gain_db (uint16_t p, bool shelf)
{
    const float c = cv_centred(p);
    const float a = shelf ? EQ_TAPER_SHELF : EQ_TAPER_BELL;
    return 15.0f * (a * c + (1.0f - a) * c * c * c);
}

uint16_t cv_eq_gain_code (uint16_t p, bool shelf, bool *cut)
{
    const float boost_db = cv_eq_gain_db(p, shelf);
    *cut = boost_db < 0.0f;
    const float a = powf(10.0f, fabsf(boost_db) / 20.0f) - 1.0f; // band VCA gain
    if (!(a > 0.0f)) return 0;                                   // 0 dB: VCA off
    return volts_to_code(EQ_V0 + EQ_V_DB * 20.0f * log10f(a));
}

/*=============================== 6-11. EQ FREQUENCY AND Q ================================*/
/* f = f0 (f1/f0)^x, so the control voltage is a straight line in x. Code at x = 0 and x = 1: */

#define LF_SHELF_CODES  92.000f, 3265.349f  // 65-550 Hz
#define LF_BELL_CODES   832.000f, 4004.016f
#define LMF_FREQ_CODES  99.632f, 3996.632f  // 180-2500 Hz
#define HMF_FREQ_CODES  114.000f, 4011.353f // 500-7000 Hz
#define HF_SHELF_CODES  838.325f, 4051.675f // 1.5-13 kHz
#define HF_BELL_CODES   78.000f, 3288.683f
#define LMF_Q_CODES     3943.000f, 56.314f  // Q = 0.5 * 10^x: 0.5 (wide) to 5 (narrow)
#define HMF_Q_CODES     3932.000f, 63.980f

/*=============================== 12-17. SENDS AND PAN ================================*/

void cv_pan_db (uint16_t p, float *left_db, float *right_db)
{
    const float theta = (cv_centred(p) + 1.0f) * 0.78539816f; // (c + 1) * pi / 4
    const float left = cosf(theta), right = sinf(theta);       // at the ends one of them is 0 (or a hair below)
    *left_db = left > 1e-6f ? 20.0f * log10f(left) : -INFINITY;
    *right_db = right > 1e-6f ? 20.0f * log10f(right) : -INFINITY;
}

/** Send level L = 10 + 60 log10(x) dB, -inf at P = 0 */
static float send_level_db (uint16_t p)
{
    return p == 0 ? -INFINITY : 10.0f + 60.0f * log10f(cv_knob(p));
}

static uint16_t send_code (float level_db)
{
    if (!(level_db > -1000.0f)) return 0; // -inf: send off
    return volts_to_code(AUX_V0 + AUX_V_DB * level_db);
}

/*=============================== 18-19. FADER AND MAIN PAN ================================*/

static const uint16_t FADER_P[] = {65535, 57140, 44380, 32007, 23612, 18160, 12956, 8828, 5433, 2543};
static const float FADER_DB[] = {12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f};
#define FADER_MARKS (sizeof(FADER_P) / sizeof(FADER_P[0]))

float cv_fader_db (uint16_t p)
{
    if (p == 0) return -INFINITY; // mute
    if (p < FADER_P[FADER_MARKS - 1]) return -60.0f - 40.0f * (float)(FADER_P[FADER_MARKS - 1] - p) / (float)FADER_P[FADER_MARKS - 1];
    for (uint8_t i = 1; i < FADER_MARKS; i++)
    {
        if (p >= FADER_P[i])
        {
            const float t = (float)(p - FADER_P[i]) / (float)(FADER_P[i - 1] - FADER_P[i]);
            return FADER_DB[i] + t * (FADER_DB[i - 1] - FADER_DB[i]);
        }
    }
    return FADER_DB[0];
}

static uint16_t output_code (float gain_db)
{
    if (!(gain_db > -1000.0f)) return CV_CODE_MAX; // -inf (mute, hard pan to the other side): fully down
    return volts_to_code(OUT_V0 + OUT_V_DB * gain_db);
}

/*=============================== THE WHOLE CHANNEL ================================*/

void cv_compute (const cv_params_t *p, cv_state_t *state, cv_result_t *out)
{
    uint16_t sw = 0;

    // 1. input gain, source and pad
    const cv_source_t source = p->source <= CV_SOURCE_HIZ ? p->source : CV_SOURCE_LINE;
    if (source != CV_SOURCE_MIC) state->pad_on = false; // the pad is only in the mic preamp
    out->code[CV_INPUT_GAIN] = input_gain_code(source, p->input_gain, &state->pad_on, &out->input_total_db);
    if (state->pad_on) sw |= RS_PAD;
    sw |= (source == CV_SOURCE_MIC) ? RS_MIC_SEL : (source == CV_SOURCE_LINE) ? RS_LINE_SEL : RS_HIZ_SEL;
#if CV_ENABLE_PHANTOM
    if (p->phantom && source == CV_SOURCE_MIC) sw |= RS_P48; // the card also refuses phantom without MIC_SEL
#endif
    if (p->phase) sw |= RS_PHASE;
    if (p->hpf) sw |= RS_HPF;

    // 2-5. EQ gain, and each band wired for boost or cut
    // EQ bypassed: every band as if its gain were at 0 dB (VCA off, wired for boost): flat
    const uint16_t flat = 32767U;
    const cv_eq_band_t lf = {p->eq_off ? flat : p->lf.gain, p->lf.freq, p->lf.q, p->lf.bell};
    const cv_eq_band_t lmf = {p->eq_off ? flat : p->lmf.gain, p->lmf.freq, p->lmf.q, p->lmf.bell};
    const cv_eq_band_t hmf = {p->eq_off ? flat : p->hmf.gain, p->hmf.freq, p->hmf.q, p->hmf.bell};
    const cv_eq_band_t hf = {p->eq_off ? flat : p->hf.gain, p->hf.freq, p->hf.q, p->hf.bell};
    bool cut;
    out->code[CV_LF_GAIN] = cv_eq_gain_code(lf.gain, !lf.bell, &cut); // LF/HF: shelf unless switched to bell
    if (cut) sw |= RS_LF_CUT;
    out->code[CV_LMF_GAIN] = cv_eq_gain_code(lmf.gain, false, &cut);
    if (cut) sw |= RS_LMF_CUT;
    out->code[CV_HMF_GAIN] = cv_eq_gain_code(hmf.gain, false, &cut);
    if (cut) sw |= RS_HMF_CUT;
    out->code[CV_HF_GAIN] = cv_eq_gain_code(hf.gain, !hf.bell, &cut);
    if (cut) sw |= RS_HF_CUT;

    // 6-11. EQ frequency (LF and HF depend on shelf or bell) and Q
    if (p->lf.bell) sw |= RS_LF_MODE;
    if (p->hf.bell) sw |= RS_HF_MODE;
    out->code[CV_LF_FREQ] = p->lf.bell ? knob_line(p->lf.freq, LF_BELL_CODES) : knob_line(p->lf.freq, LF_SHELF_CODES);
    out->code[CV_LMF_FREQ] = knob_line(p->lmf.freq, LMF_FREQ_CODES);
    out->code[CV_HMF_FREQ] = knob_line(p->hmf.freq, HMF_FREQ_CODES);
    out->code[CV_HF_FREQ] = p->hf.bell ? knob_line(p->hf.freq, HF_BELL_CODES) : knob_line(p->hf.freq, HF_SHELF_CODES);
    out->code[CV_LMF_Q] = knob_line(p->lmf.q, LMF_Q_CODES);
    out->code[CV_HMF_Q] = knob_line(p->hmf.q, HMF_Q_CODES);

    // 12-17. sends: 1 and 2 stereo with pan, 3 one CV for both VCAs, 4 single VCA
    float left, right;
    const float send1 = send_level_db(p->aux_level[0]);
    cv_pan_db(p->aux_pan[0], &left, &right);
    out->code[CV_AUX1_L] = send_code(send1 + left);
    out->code[CV_AUX1_R] = send_code(send1 + right);
    const float send2 = send_level_db(p->aux_level[1]);
    cv_pan_db(p->aux_pan[1], &left, &right);
    out->code[CV_AUX2_L] = send_code(send2 + left);
    out->code[CV_AUX2_R] = send_code(send2 + right);
    out->code[CV_AUX3] = send_code(send_level_db(p->aux_level[2]));
    out->code[CV_AUX4] = send_code(send_level_db(p->aux_level[3]));
    if (p->sends_off) // all sends at their lowest gain; the levels themselves are kept
    {
        out->code[CV_AUX1_L] = out->code[CV_AUX1_R] = out->code[CV_AUX2_L] = out->code[CV_AUX2_R] = 0;
        out->code[CV_AUX3] = out->code[CV_AUX4] = 0;
    }

    // 18-19. fader and main pan; past the output VCAs' range the rest goes to the compressor (E)
    const float fader = cv_fader_db(p->fader);
    cv_pan_db(p->pan, &left, &right);
    const float gain_l = fader + left, gain_r = fader + right;
    out->code[CV_OUT_L] = output_code(gain_l);
    out->code[CV_OUT_R] = output_code(gain_r);
    const float louder = gain_l > gain_r ? gain_l : gain_r;
    out->comp_extra_db = isinf(louder) ? 0.0f : fminf(0.0f, louder - OUT_FLOOR_DB);

    // insert: PRE_LCV high takes the insert send after the EQ, low before it ("pre")
#if CV_ENABLE_INSERT
    if (p->insert) sw |= RS_INSERT;
    if (!p->insert_pre) sw |= RS_PRE;
#else
    sw |= RS_PRE; // insert out, send after the EQ
#endif

    // mix bus: off when muted, when the fader is fully down, or when another channel is soloed and this one
    // isn't. Only the mix bus: the sends keep going.
    if (!p->muted && p->fader > 0 && (p->soloed || !p->other_soloed)) sw |= RS_MIX_ON;

    out->switches = sw;
}
