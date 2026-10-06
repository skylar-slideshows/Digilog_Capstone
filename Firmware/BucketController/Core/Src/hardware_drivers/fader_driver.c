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

// ADC STUFF
extern ADC_HandleTypeDef FADER_ADC; // from main.c
uint32_t adc_buf[CHANNELS] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

// TIMER STUFF
extern TIM_HandleTypeDef FADER1_TIM_HANDLE; // from main.c
extern TIM_HandleTypeDef FADER3_TIM_HANDLE; // from main.c


uint32_t fader_touched_countdown[CHANNELS] = {0};

static bool is_fader_touched (fader_info_t *info)
{
    bool hw_touched = HAL_GPIO_ReadPin(info->touch_sensor_port, info->touch_sensor_pin) == GPIO_PIN_SET;
    if (hw_touched)
    {
        fader_touched_countdown[info->channel] = FADER_UNTOUCHED_DELAY;
    }
    return hw_touched || fader_touched_countdown[info->channel] > 0;
}

void poll_faders (void)
{
    HAL_ADC_Start(&FADER_ADC);

    for (int i = 0; i < CHANNELS; i++)
    {
        if (fader_touched_countdown[i] > 0)
        {
            fader_touched_countdown[i]--;
        }

        if (HAL_ADC_PollForConversion(&FADER_ADC, 10) == HAL_OK)
        {
            uint16_t read = HAL_ADC_GetValue(&FADER_ADC) << 4; // Multiply up to 16bit
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

    HAL_ADC_Stop(&FADER_ADC);
}

static uint16_t get_physical_fader_position (fader_info_t *info) { return adc_buf[info->channel]; }

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint32_t timer_a_channel;
    uint32_t timer_b_channel;
} motor_hw_info;

const motor_hw_info motor_hw[CHANNELS] = {
    (motor_hw_info){.timer = &FADER1_TIM_HANDLE, .timer_a_channel = FADER1_A_TIM_CHAN, .timer_b_channel = FADER1_B_TIM_CHAN},
    (motor_hw_info){.timer = &FADER2_TIM_HANDLE, .timer_a_channel = FADER2_A_TIM_CHAN, .timer_b_channel = FADER2_B_TIM_CHAN},
    (motor_hw_info){.timer = &FADER3_TIM_HANDLE, .timer_a_channel = FADER3_A_TIM_CHAN, .timer_b_channel = FADER3_B_TIM_CHAN},
    (motor_hw_info){.timer = &FADER4_TIM_HANDLE, .timer_a_channel = FADER4_A_TIM_CHAN, .timer_b_channel = FADER4_B_TIM_CHAN},
};

uint32_t fader_pwm_period[CHANNELS];

typedef struct
{
    float deviation_integral;
    float last_pos; //!< For velocity
    float averaged_velocity;
} motor_piv_info;

motor_piv_info motor_pivs[CHANNELS]; //!< Control loop info for each fader

void init_faders (void)
{
    poll_faders();
    for (uint8_t i = 0; i < CHANNELS; i++)
    {
        HAL_TIM_PWM_Start(motor_hw[i].timer, motor_hw[i].timer_a_channel);
        HAL_TIM_PWM_Start(motor_hw[i].timer, motor_hw[i].timer_b_channel);

        fader_pwm_period[i] = __HAL_TIM_GET_AUTORELOAD(motor_hw[i].timer);

        motor_pivs[i] = (motor_piv_info){.deviation_integral = 0, .last_pos = adc_buf[i]};
    }
}


static void update_motor_power (
    fader_info_t *info,
    bool powered, //!< Whether the motor should be powered at all (disable for free motion)
    int32_t power_pwm_period
)
{
    const motor_hw_info *hw = &(motor_hw[info->channel]);

    if (!powered)
    {
        __HAL_TIM_SET_COMPARE(hw->timer, hw->timer_a_channel, 0);
        __HAL_TIM_SET_COMPARE(hw->timer, hw->timer_b_channel, 0);
    }

    const uint32_t main_channel = power_pwm_period > 0 ? hw->timer_a_channel : hw->timer_b_channel;
    const uint32_t low_channel = power_pwm_period <= 0 ? hw->timer_a_channel : hw->timer_b_channel;

    power_pwm_period =
        power_pwm_period < 0 ? (power_pwm_period == INT32_MIN ? INT32_MAX : -power_pwm_period) : power_pwm_period;

    // __HAL_TIM_SET_COMPARE(hw->timer, low_channel, 0);
    // __HAL_TIM_SET_COMPARE(hw->timer, main_channel, power_pwm_period);
    __HAL_TIM_SET_COMPARE(hw->timer, low_channel, fader_pwm_period[info->channel] - power_pwm_period);
    __HAL_TIM_SET_COMPARE(hw->timer, main_channel, fader_pwm_period[info->channel]);
}

/**
 * @brief Feedback control based on motor target and position
 * Feedback control to move the motor towards a target position;
 * currently just uses proportional deviation with no derivative or integral
 */
static int32_t get_motor_power (fader_info_t *info, fader_state_t *state)
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

void update_fader (fader_info_t *info, fader_state_t *old_state, fader_state_t *new_state)
{
    fader_state_t new_state_i = *old_state;
    if (is_fader_touched(info))
    {
        new_state_i.movement_mode = FADER_UNPOWERED;
        new_state_i.position = get_physical_fader_position(info);
        get_motor_power(info, &new_state_i); // for jitter fixes or whatever
        update_motor_power(info, false, 0);
    }
    else
    {
        new_state_i.movement_mode = FADER_MOVE_OR_HOLD_TARGET;
        update_motor_power(info, true, get_motor_power(info, &new_state_i));
    }

    *new_state = new_state_i;
}
