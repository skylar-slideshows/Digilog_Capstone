/**
  **********************************************************************************
  * COMPRESSOR ENGINE - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file comp/comp_engine.c
  * @brief See comp_engine.h. No hardware here: the compressor task feeds it samples.
  **********************************************************************************
*/

#include "comp/comp_engine.h"
#include <math.h>

#define ENV_FLOOR_V 0.0005f // below about half an ADC step: treat as silence (-53.7 dBu) and keep log() finite

static float fraction (uint16_t p) { return (float)p / 65535.0f; }

/*=============================== KNOB LAWS ================================*/

float comp_input_db (uint16_t p) { return COMP_INPUT_MIN_DB + (COMP_INPUT_MAX_DB - COMP_INPUT_MIN_DB) * fraction(p); }

float comp_output_db (uint16_t p) { return COMP_OUTPUT_MIN_DB + (COMP_OUTPUT_MAX_DB - COMP_OUTPUT_MIN_DB) * fraction(p); }

float comp_threshold_dbu (uint16_t p)
{
    return COMP_THRESH_MIN_DBU + (COMP_THRESH_MAX_DBU - COMP_THRESH_MIN_DBU) * fraction(p);
}

float comp_attack_ms (uint16_t p) // log taper: equal turns, equal ratios of time
{
    return COMP_ATTACK_MIN_MS * powf(COMP_ATTACK_MAX_MS / COMP_ATTACK_MIN_MS, fraction(p));
}

float comp_release_ms (uint16_t p)
{
    return COMP_RELEASE_MIN_MS * powf(COMP_RELEASE_MAX_MS / COMP_RELEASE_MIN_MS, fraction(p));
}

/* RATIO works on the curve's slope above the threshold, 1 - 1/ratio: 0 fully left (1:1), 0.75 in the
   middle (4:1), 1 fully right (inf:1), easing in so the low ratios get most of the turn. */
float comp_slope (uint16_t p)
{
    const float x = 1.0f - fraction(p);
    return 1.0f - x * x;
}

float comp_ratio (uint16_t p)
{
    const float s = comp_slope(p);
    return s >= 1.0f ? INFINITY : 1.0f / (1.0f - s);
}

uint16_t comp_p_from_fraction (float x)
{
    if (x <= 0.0f) return 0;
    if (x >= 1.0f) return 65535;
    return (uint16_t)lroundf(x * 65535.0f);
}

/*=============================== COEFFICIENTS ================================*/

/** One-pole coefficient for a time constant (the time to get 63% of the way) at a sample rate */
static float one_pole (float ms, float sample_hz)
{
    const float samples = ms * 0.001f * sample_hz;
    return samples > 0.0f ? expf(-1.0f / samples) : 0.0f;
}

void comp_engine_coeffs (comp_coeffs_t *c, const comp_knobs_t *k, float sample_hz)
{
    c->input_db = comp_input_db(k->input);
    c->threshold_dbu = comp_threshold_dbu(k->threshold);
    c->slope = comp_slope(k->ratio);
    c->attack_coeff = one_pole(comp_attack_ms(k->attack), sample_hz);
    c->release_coeff = one_pole(comp_release_ms(k->release), sample_hz);
    c->rms_coeff = one_pole(COMP_RMS_MS, sample_hz);
    c->rms = k->rms;
    c->bypass = k->bypass;
}

/*=============================== PER SAMPLE ================================*/

float comp_fast_log2 (float x)
{
    // exponent from the float's bits, then a polynomial for the mantissa (1 <= m < 2)
    union { float f; uint32_t i; } u = {x};
    const float e = (float)((int32_t)((u.i >> 23) & 0xFFU) - 127);
    u.i = (u.i & 0x007FFFFFU) | 0x3F800000U;
    const float t = u.f - 1.0f; // 0 <= t < 1
    // log2(1 + t), degree-5 near-minimax fit: worst error 1.4e-5 (0.0001 dB)
    return e + (((((0.043929100f * t - 0.189834428f) * t + 0.411564148f) * t - 0.707254899f) * t + 1.441592392f) * t + 0.000014373f);
}

float comp_engine_target_gr (const comp_coeffs_t *c, float level_dbu)
{
    const float over = level_dbu - c->threshold_dbu;
    return over > 0.0f ? c->slope * over : 0.0f; // hard knee
}

float comp_engine_step (const comp_coeffs_t *c, comp_state_t *s, float env_volts, float extra_db)
{
    const float v = env_volts > ENV_FLOOR_V ? env_volts : ENV_FLOOR_V;
    const float v2 = v * v;
    s->ms = v2 + c->rms_coeff * (s->ms - v2); // always, so switching to RMS doesn't jump
    float level;
    if (c->rms)
    {
        // 10 log10(2 ms) puts a sine's RMS where its peak would read; then referenced to 0 dBu
        level = 3.0103f * comp_fast_log2(2.0f * s->ms * (1.0f / (COMP_ENV_0DBU_V * COMP_ENV_0DBU_V)));
    }
    else
    {
        level = 6.0205999f * comp_fast_log2(v * (1.0f / COMP_ENV_0DBU_V)); // 20 log10
    }
    level += c->input_db;

    const float target = comp_engine_target_gr(c, level);
    if (c->rms)
    {
        // RMS level is already smooth: attack while the gain reduction rises, release while it falls
        const float coeff = target > s->gr_db ? c->attack_coeff : c->release_coeff;
        s->gr_db = target + coeff * (s->gr_db - target);
    }
    else
    {
        // the envelope is the rectified audio itself, dropping to nothing between peaks: hold each peak's
        // gain reduction and let it fall at the release rate, then smooth its rises with the attack
        const float held = target + c->release_coeff * (s->peak_gr - target);
        s->peak_gr = target > held ? target : held;
        s->gr_db = s->peak_gr + c->attack_coeff * (s->gr_db - s->peak_gr);
    }

    // INPUT only raises what the detector sees (the same job as lowering THRESH, for people used to
    // fixed-threshold compressors); the audio isn't raised by it
    float gain = c->bypass ? extra_db : extra_db - s->gr_db;
    if (gain < COMP_GAIN_MIN_DB) gain = COMP_GAIN_MIN_DB;
    if (gain > COMP_GAIN_MAX_DB) gain = COMP_GAIN_MAX_DB;
    return gain;
}

/*=============================== TO THE DAC ================================*/

float comp_engine_dac_volts (float gain_db, float scaler_k)
{
    return (COMP_SCALER_OFFSET_V + COMP_SCALER_V_PER_DB * gain_db) / scaler_k;
}

uint16_t comp_engine_dac_code (float gain_db, float scaler_k, float dac_vref)
{
    const float code = comp_engine_dac_volts(gain_db, scaler_k) * (4095.0f / dac_vref) + 0.5f;
    if (code <= 0.0f) return 0;
    if (code >= 4095.0f) return 4095;
    return (uint16_t)code;
}
