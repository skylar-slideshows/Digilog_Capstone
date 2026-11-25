#include <stdbool.h>
#include <stdint.h>
#include "hardware_drivers/fader_driver.h"
#include "control_integration/control_interface.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"

#define POWER_MAX_INV_COEF 2

#define FADER_PROPORTIONAL_COEF 0.00012f
#define FADER_DERIVATIVE_COEF 0.4f
#define FADER_INTEGRAL_COEF 0
#define FADER_INTEGRAL_MAX 2000
// #define FADER_PRELOAD_DYN 0.02
// #define FADER_PRELOAD_STA 0.04
#define FADER_PRELOAD_DYN -0.02f
#define FADER_PRELOAD_STA 0
#define DYNAMIC_MIN_VELOCITY 4000

#define FADER_UNTOUCHED_DELAY 4000

#define FADER_AVG_DERIVATIVE_AMOUNT 0.4f
#define FADER_AVG_PROPORTIONAL_AMOUNT_INV_COEF 5

// What each fader is wired to (CONFIG.h): motor A and B can be on different timers, positions on two ADCs
extern ADC_HandleTypeDef FADER0_ADC, FADER1_ADC, FADER2_ADC, FADER3_ADC;         // from main.c
extern TIM_HandleTypeDef FADER0_A_TIM, FADER0_B_TIM, FADER1_A_TIM, FADER1_B_TIM; // from main.c
extern TIM_HandleTypeDef FADER2_A_TIM, FADER2_B_TIM, FADER3_A_TIM, FADER3_B_TIM;

typedef struct
{
    TIM_HandleTypeDef *tim_a;
    uint32_t chan_a;
    TIM_HandleTypeDef *tim_b;
    uint32_t chan_b;
    ADC_HandleTypeDef *adc;
    uint32_t adc_chan;
} fader_hw_t;

static const fader_hw_t FADER_HW[CHANNELS] = {
    {&FADER0_A_TIM, FADER0_A_CHAN, &FADER0_B_TIM, FADER0_B_CHAN, &FADER0_ADC, FADER0_ADC_CHAN},
    {&FADER1_A_TIM, FADER1_A_CHAN, &FADER1_B_TIM, FADER1_B_CHAN, &FADER1_ADC, FADER1_ADC_CHAN},
    {&FADER2_A_TIM, FADER2_A_CHAN, &FADER2_B_TIM, FADER2_B_CHAN, &FADER2_ADC, FADER2_ADC_CHAN},
    {&FADER3_A_TIM, FADER3_A_CHAN, &FADER3_B_TIM, FADER3_B_CHAN, &FADER3_ADC, FADER3_ADC_CHAN},
};

uint32_t adc_buf[CHANNELS] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};


uint32_t fader_touched_countdown[CHANNELS] = {0};

static bool is_fader_touched (const fader_info_t *info)
{
    bool hw_touched = HAL_GPIO_ReadPin(info->touch_sensor_port, info->touch_sensor_pin) == GPIO_PIN_SET;
    if (hw_touched)
    {
        fader_touched_countdown[info->channel] = FADER_UNTOUCHED_DELAY;
    }
    return hw_touched || fader_touched_countdown[info->channel] > 0;
}

/**
 * The ADCs the faders use are set to one conversion at a time, whatever CubeMX ranked, and calibrated.
 * Only waits on loop counts, never HAL_GetTick(), so it's safe before the scheduler starts.
 */
static void setup_fader_adcs (void)
{
    for (uint8_t i = 0; i < CHANNELS; i++)
    {
        ADC_HandleTypeDef *h = FADER_HW[i].adc;
        bool done = false;
        for (uint8_t j = 0; j < i; j++) done |= FADER_HW[j].adc == h; // each ADC once
        if (done) continue;
        h->Init.ScanConvMode = ADC_SCAN_DISABLE;
        h->Init.NbrOfConversion = 1;
        h->Init.ContinuousConvMode = DISABLE;
        h->Init.DiscontinuousConvMode = DISABLE;
        h->Init.ExternalTrigConv = ADC_SOFTWARE_START;
        h->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
        h->Init.DMAContinuousRequests = DISABLE;
        h->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
        HAL_ADC_Init(h);
        HAL_ADCEx_Calibration_Start(h, ADC_SINGLE_ENDED);
    }
}

/** One fader's position, 12-bit, or -1 if its ADC didn't answer. Runs in the fader poller task. */
static int32_t read_fader_adc (const fader_hw_t *hw)
{
    ADC_ChannelConfTypeDef c = {0};
    c.Channel = hw->adc_chan;
    c.Rank = ADC_REGULAR_RANK_1;
    c.SamplingTime = FADER_ADC_SAMPLETIME;
    c.SingleDiff = ADC_SINGLE_ENDED;
    c.OffsetNumber = ADC_OFFSET_NONE;
    c.Offset = 0;
    if (HAL_ADC_ConfigChannel(hw->adc, &c) != HAL_OK) return -1;
    if (HAL_ADC_Start(hw->adc) != HAL_OK) return -1;
    int32_t value = -1;
    for (uint32_t spins = 0; spins < 200000U; spins++) // a conversion is tens of microseconds
    {
        if (__HAL_ADC_GET_FLAG(hw->adc, ADC_FLAG_EOC))
        {
            value = (int32_t)HAL_ADC_GetValue(hw->adc);
            break;
        }
    }
    HAL_ADC_Stop(hw->adc);
    return value;
}

void poll_faders (void)
{
    for (int i = 0; i < CHANNELS; i++)
    {
        if (fader_touched_countdown[i] > 0)
        {
            fader_touched_countdown[i]--;
        }

        const int32_t raw = read_fader_adc(&FADER_HW[i]);
        if (raw >= 0)
        {
            uint16_t read = (uint16_t)(raw << 4); // Multiply up to 16bit
            if (fader_touched_countdown[i] > 0)
            { // disable smoothing if touched
                adc_buf[i] = read;
            }

            if (adc_buf[i] == UINT32_MAX)
            {
                adc_buf[i] = read;
                continue;
            }
            uint32_t last = adc_buf[i];
            last /= FADER_AVG_PROPORTIONAL_AMOUNT_INV_COEF;

            uint32_t next = read - (read / FADER_AVG_PROPORTIONAL_AMOUNT_INV_COEF);

            adc_buf[i] = next + last;
        }
    }
}

static uint16_t get_physical_fader_position (const fader_info_t *info) { return adc_buf[info->channel]; }

uint32_t fader_pwm_period[CHANNELS];   //!< motor A's timer period: the scale the control loop's power is in
static uint32_t s_period_b[CHANNELS];   //!< motor B's timer period (the same unless the timers differ)

typedef struct
{
    float deviation_integral;
    float last_pos; //!< For velocity
    float averaged_velocity;
} motor_piv_info;

motor_piv_info motor_pivs[CHANNELS]; //!< Control loop info for each fader

static fader_state_t fader_cache[CHANNELS]; //!< Last state of each fader, indexed by channel

void init_faders (void)
{
#if FADERS_CONNECTED
    setup_fader_adcs(); // positions are first read by the fader poller task (poll_faders), once running
#endif
    for (uint8_t i = 0; i < CHANNELS; i++)
    {
        HAL_TIM_PWM_Start(FADER_HW[i].tim_a, FADER_HW[i].chan_a);
        HAL_TIM_PWM_Start(FADER_HW[i].tim_b, FADER_HW[i].chan_b);

        fader_pwm_period[i] = __HAL_TIM_GET_AUTORELOAD(FADER_HW[i].tim_a);
        s_period_b[i] = __HAL_TIM_GET_AUTORELOAD(FADER_HW[i].tim_b);

        motor_pivs[i] = (motor_piv_info){.deviation_integral = 0, .last_pos = 0};
        fader_cache[i] = DEFAULT_FADER_STATE;
    }
}


static void update_motor_power (
    const fader_info_t *info,
    bool powered, //!< Whether the motor should be powered at all (disable for free motion)
    int32_t power_pwm_period
)
{
    const fader_hw_t *hw = &(FADER_HW[info->channel]);
    const uint32_t period_a = fader_pwm_period[info->channel], period_b = s_period_b[info->channel];

    if (!powered)
    {
        __HAL_TIM_SET_COMPARE(hw->tim_a, hw->chan_a, 0);
        __HAL_TIM_SET_COMPARE(hw->tim_b, hw->chan_b, 0);
        return;
    }

    // one side held fully on, the other PWM'd: the difference drives the motor (power is in motor A's period)
    const bool forward = power_pwm_period > 0;
    uint32_t power =
        power_pwm_period < 0 ? (power_pwm_period == INT32_MIN ? INT32_MAX : (uint32_t)(-power_pwm_period)) : (uint32_t)power_pwm_period;
    if (power > period_a) power = period_a;

    if (forward)
    {
        __HAL_TIM_SET_COMPARE(hw->tim_a, hw->chan_a, period_a);
        __HAL_TIM_SET_COMPARE(hw->tim_b, hw->chan_b, period_b - (uint32_t)((uint64_t)power * period_b / period_a));
    }
    else
    {
        __HAL_TIM_SET_COMPARE(hw->tim_b, hw->chan_b, period_b);
        __HAL_TIM_SET_COMPARE(hw->tim_a, hw->chan_a, period_a - power);
    }
}

/**
 * @brief Feedback control based on motor target and position
 * Feedback control to move the motor towards a target position;
 * currently just uses proportional deviation with no derivative or integral
 */
static int32_t get_motor_power (const fader_info_t *info, const fader_state_t *state)
{
    motor_piv_info *chan = &(motor_pivs[info->channel]);
    const int32_t where_it_is = get_physical_fader_position(info);
    const int32_t where_it_isnt = state->position;

    const int32_t where_it_was = chan->last_pos;

    const float integral = chan->deviation_integral;

    int32_t deviation = where_it_isnt - where_it_is;
    int32_t instant_velocity = where_it_was - where_it_is;

    chan->last_pos = where_it_is;
    chan->deviation_integral += deviation * FADER_INTEGRAL_COEF;

    if (chan->deviation_integral > FADER_INTEGRAL_MAX)
    {
        chan->deviation_integral = FADER_INTEGRAL_MAX;
    }
    if (chan->deviation_integral < -FADER_INTEGRAL_MAX)
    {
        chan->deviation_integral = -FADER_INTEGRAL_MAX;
    }

    chan->averaged_velocity *= FADER_AVG_DERIVATIVE_AMOUNT;
    chan->averaged_velocity += (1.0f - FADER_AVG_DERIVATIVE_AMOUNT) * instant_velocity;

    if(is_fader_touched(info)){
        chan->averaged_velocity = 0;
        chan->deviation_integral = 0;
    }

    int64_t period = fader_pwm_period[info->channel];

    int64_t product = period * deviation;
    product *= FADER_PROPORTIONAL_COEF;

    product += chan->averaged_velocity * FADER_DERIVATIVE_COEF;
    product += integral;

    double fader_preload = FADER_PRELOAD_STA;
    if (chan->averaged_velocity > DYNAMIC_MIN_VELOCITY || chan->averaged_velocity < -DYNAMIC_MIN_VELOCITY)
    {
        fader_preload = FADER_PRELOAD_DYN;
    }

    if (product > 0)
    {
        product += fader_preload * period;
        if (product < 0)
        {
            product = 0;
        }
    }
    else if (product < 0)
    {
        product -= fader_preload * period;
        if (product > 0)
        {
            product = 0;
        }
    }


    if (product > period / POWER_MAX_INV_COEF)
    {
        product = period / POWER_MAX_INV_COEF;
    }
    else if (product < -period / POWER_MAX_INV_COEF)
    {
        product = -period / POWER_MAX_INV_COEF;
    }

    return product;
}

void update_fader (const fader_info_t *info)
{
    fader_state_t *state = &(fader_cache[info->channel]);
    if (is_fader_touched(info))
    {
        state->movement_mode = FADER_UNPOWERED;
        state->position = get_physical_fader_position(info);
        get_motor_power(info, state); // for jitter fixes or whatever
        update_motor_power(info, false, 0);
    }
    else
    {
        state->movement_mode = FADER_MOVE_OR_HOLD_TARGET;
        update_motor_power(info, true, get_motor_power(info, state));
    }
}

fader_state_t get_fader_state (const fader_info_t *info) { return fader_cache[info->channel]; }

void set_fader_target (const fader_info_t *info, uint16_t position) { fader_cache[info->channel].position = position; }
