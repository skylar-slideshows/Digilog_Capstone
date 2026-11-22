/**
  **********************************************************************************
  * COMPRESSOR - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/compressor.c
  * @brief The hardware side of the hybrid compressor (comp_engine.c does the maths).
  *
  * Per channel:  comp card envelope (ENV_ADC) -> ADC4 -> comp_engine -> DAC -> COMP_CVn
  *               -> routing board J2_1 (jumpered) -> comp card scaler -> COMP_CV -> VCA E-
  *
  * Outputs (from the .ioc):
  *   channel 1  COMP_CV0  PB11  OPAMP6 follower <- DAC3 channel 1 (internal)
  *   channel 2  COMP_CV1  PB1   OPAMP3 follower <- DAC3 channel 2 (internal)
  *   channel 3  COMP_CV2  PB12  OPAMP4 follower <- DAC4 channel 1 (internal)
  *   channel 4  COMP_CV3  PA4   DAC1 channel 1, its own output buffer (no op-amp: it can't go below
  *                              about 0.2 V, so channel 4's deepest gain reduction is about -37 dB)
  * Inputs: ADC4 ranks 1-4 = IN7-IN10 = PD10-PD13 = COMP_ADC0-3 (see COMP_ADC_RANK).
  *
  * CUBEMX SETUP for COMP_LOOP_ENABLED (CONFIG.h); until then the outputs just hold 0 dB:
  *   TIM6:  activated. Prescaler 0, Counter Period 5311 (170 MHz / 5312 = 32.0 kHz),
  *          Trigger Event Selection (TRGO) = Update Event. No interrupt needed.
  *   ADC4:  External Trigger Conversion Source = Timer 6 Trigger Out event, rising edge.
  *          Scan conversion, 4 ranks (IN7, IN8, IN9, IN10, as now), Continuous Conversion Disabled,
  *          DMA Continuous Requests Enabled, Sampling Time 12.5 cycles (the cards' 47 nF caps hold
  *          the input steady, so this is plenty).
  *          DMA Settings: add ADC4, Mode Circular, Data Width Half Word (peripheral and memory),
  *          memory increment on.
  *   NVIC:  that DMA channel's interrupt enabled. It makes no FreeRTOS calls, so it can be a high
  *          priority (a low number, e.g. 2) for steady timing.
  **********************************************************************************
*/

#include "FreeRTOS_apps/compressor.h"
#include "comp/comp_engine.h"
#include "CONFIG.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern DAC_HandleTypeDef hdac1, hdac3, hdac4;
extern OPAMP_HandleTypeDef hopamp3, hopamp4, hopamp6;
extern ADC_HandleTypeDef hadc4;
#if COMP_LOOP_ENABLED
extern TIM_HandleTypeDef htim6;
#endif

/** Each channel's DAC output, in channel order */
static const struct
{
    DAC_HandleTypeDef *dac;
    uint32_t channel;
    OPAMP_HandleTypeDef *follower; //!< NULL: none (DAC1 drives its pin through its own buffer)
} COMP_OUT[CHANNELS] = {
    {&hdac3, DAC_CHANNEL_1, &hopamp6}, // channel 1: PB11
    {&hdac3, DAC_CHANNEL_2, &hopamp3}, // channel 2: PB1
    {&hdac4, DAC_CHANNEL_1, &hopamp4}, // channel 3: PB12
    {&hdac1, DAC_CHANNEL_1, NULL},     // channel 4: PA4
};

/** Which ADC4 rank (0-3) carries each channel's envelope: rank 1 = COMP_ADC0 (PD10) ... */
static const uint8_t COMP_ADC_RANK[CHANNELS] = {0, 1, 2, 3};

#define SCALER_K   (10000.0f / COMP_R7_OHMS) // the comp card scaler's gain from DAC volts
#define ADC_TO_V   (COMP_DAC_VREF / 4095.0f)

static float s_sample_hz = (float)COMP_SAMPLE_HZ;

/* Coefficients, two copies per channel: the I2C task fills the spare one, then switches the sampling
   interrupt over to it, so the interrupt never sees a half-written set. */
static comp_coeffs_t s_coeffs[2][CHANNELS];
static volatile uint8_t s_live[CHANNELS];
static comp_knobs_t s_knobs[CHANNELS];
static bool s_have_knobs[CHANNELS];

static volatile float s_extra_db[CHANNELS];  // E from cv_mapping: gain the main VCA CVs couldn't reach
static comp_state_t s_state[CHANNELS];       // only the sampling interrupt writes these
static uint16_t s_env_raw[CHANNELS];         // ADC4 DMA: one scan of the four envelopes per TIM6 tick
static volatile uint32_t s_isr_cycles_max;

/** Fills k field by field after zeroing it, so its padding is known and memcmp works on it */
static void knobs_of (comp_knobs_t *k, const comp_control_t *c, bool bypassed)
{
    memset(k, 0, sizeof(*k));
    k->input = c->in_gain;
    k->attack = c->attack_time;
    k->threshold = c->threshold;
    k->release = c->release_time;
    k->ratio = c->ratio;
    k->rms = c->rms;
    k->bypass = bypassed;
}

void compressor_set_knobs (uint8_t channel, const comp_control_t *knobs, bool bypassed)
{
    if (channel >= CHANNELS || knobs == NULL) return;
    comp_knobs_t k;
    knobs_of(&k, knobs, bypassed);
    if (s_have_knobs[channel] && memcmp(&k, &s_knobs[channel], sizeof(k)) == 0) return;
    s_knobs[channel] = k;
    s_have_knobs[channel] = true;

    const uint8_t spare = (uint8_t)(s_live[channel] ^ 1U);
    comp_engine_coeffs(&s_coeffs[spare][channel], &k, s_sample_hz);
    __DMB(); // the new set is complete before the interrupt can switch to it
    s_live[channel] = spare;
}

void compressor_set_extra_gain (uint8_t channel, float extra_db)
{
    if (channel < CHANNELS) s_extra_db[channel] = extra_db; // one float: the interrupt reads it whole
}

float compressor_gain_reduction_db (uint8_t channel)
{
    return channel < CHANNELS ? s_state[channel].gr_db : 0.0f;
}

uint32_t compressor_isr_cycles_max (void)
{
    return s_isr_cycles_max;
}

static void write_dac (uint8_t channel, uint16_t code)
{
    HAL_DAC_SetValue(COMP_OUT[channel].dac, COMP_OUT[channel].channel, DAC_ALIGN_12B_R, code);
}

#if COMP_LOOP_ENABLED
/** Every TIM6 tick, once ADC4 has scanned the four envelopes: run all four compressors */
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC4) return;
    const uint32_t t0 = DWT->CYCCNT;
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        const float env = (float)s_env_raw[COMP_ADC_RANK[ch]] * ADC_TO_V;
        const float gain = comp_engine_step(&s_coeffs[s_live[ch]][ch], &s_state[ch], env, s_extra_db[ch]);
        write_dac(ch, comp_engine_dac_code(gain, SCALER_K, COMP_DAC_VREF));
    }
    const uint32_t cycles = DWT->CYCCNT - t0;
    if (cycles > s_isr_cycles_max) s_isr_cycles_max = cycles;
}

/** TIM6's actual tick rate, so the attack and release times come out right */
static float timer_rate_hz (void)
{
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1) clock *= 2U; // timers run at twice a divided APB clock
    const uint32_t ticks = (htim6.Instance->PSC + 1U) * (htim6.Instance->ARR + 1U);
    return ticks ? (float)clock / (float)ticks : (float)COMP_SAMPLE_HZ;
}
#endif

void compressor_init (void)
{
    // the cycle counter (also turned on by the displays) for the load measurement
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Outputs up at 0 dB: no gain reduction, and never the 0 V that a comp card reads as -44 dB. Without
    // the sampling running, E (the main CVs' overflow) isn't applied either; it's 0 in normal use.
    const uint16_t unity = comp_engine_dac_code(0.0f, SCALER_K, COMP_DAC_VREF);
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        write_dac(ch, unity);
        HAL_DAC_Start(COMP_OUT[ch].dac, COMP_OUT[ch].channel);
    }
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        // Factory trimming (the .ioc's setting) only. HAL_OPAMP_SelfCalibrate() waits with HAL_Delay(),
        // and this runs before the scheduler starts, when FreeRTOS has the tick interrupt masked:
        // HAL_Delay() never returns there and the whole console hangs. The factory trim leaves a few mV
        // of offset, which is under 0.1 dB at the VCAs.
        OPAMP_HandleTypeDef *op = COMP_OUT[ch].follower;
        if (op != NULL) HAL_OPAMP_Start(op);
    }

#if COMP_LOOP_ENABLED
    s_sample_hz = timer_rate_hz();
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        compressor_set_knobs(ch, &(control_defaults()->comp_control), false);
        s_state[ch].gr_db = 0.0f;
    }
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
    if (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)s_env_raw, CHANNELS) != HAL_OK || hadc4.DMA_Handle == NULL)
    {
#if DEVELOPER_MODE
        printf("\r\nCompressor: ADC4 DMA isn't set up (see compressor.c); outputs stay at 0 dB\n");
#endif
        return;
    }
    HAL_TIM_Base_Start(&htim6);
#if DEVELOPER_MODE
    printf("\r\nCompressor: running at %lu Hz, 0 dB = DAC code %u\n", (unsigned long)(s_sample_hz + 0.5f), (unsigned)unity);
#endif
#else
    (void)s_env_raw;
    (void)s_coeffs;
#if DEVELOPER_MODE
    printf("\r\nCompressor: outputs at 0 dB (DAC code %u); sampling off until COMP_LOOP_ENABLED\n", (unsigned)unity);
#endif
#endif
}
