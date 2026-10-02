#include <stdbool.h>
#include <stdint.h>
#include "hardware_drivers/fader_driver.h"
#include "control_integration/control_interface.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"

#define FADER_DEVIATION_UNTIL_MOTOR_MAX 100

// ADC STUFF
extern ADC_HandleTypeDef FADER_ADC; // from main.c
uint32_t adc_buf[CHANNELS];

// TIMER STUFF
extern TIM_HandleTypeDef FADER1_TIM_HANDLE; // from main.c
extern TIM_HandleTypeDef FADER3_TIM_HANDLE; // from main.c

void poll_faders (void)
{
    HAL_ADC_Start(&FADER_ADC);

    for (int i = 0; i < CHANNELS; i++)
    {
        if (HAL_ADC_PollForConversion(&FADER_ADC, 10) == HAL_OK)
        {
            adc_buf[i] = HAL_ADC_GetValue(&FADER_ADC);
        }
    }

    HAL_ADC_Stop(&FADER_ADC);
}

static uint16_t get_physical_fader_position (fader_info_t *info) {
    return adc_buf[info->channel];
}

static bool is_fader_touched (fader_info_t *info)
{
    return HAL_GPIO_ReadPin(info->touch_sensor_port, info->touch_sensor_pin) == GPIO_PIN_SET;
}

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint8_t timer_a_channel;
    uint8_t timer_b_channel;
} motor_hw_info;

const motor_hw_info motor_hw[CHANNELS] = {
    (motor_hw_info){.timer = &FADER1_TIM_HANDLE, .timer_a_channel = FADER1_A_TIM_AF, .timer_b_channel = FADER1_B_TIM_AF},
    (motor_hw_info){.timer = &FADER2_TIM_HANDLE, .timer_a_channel = FADER2_A_TIM_AF, .timer_b_channel = FADER2_B_TIM_AF},
    (motor_hw_info){.timer = &FADER3_TIM_HANDLE, .timer_a_channel = FADER3_A_TIM_AF, .timer_b_channel = FADER3_B_TIM_AF},
    (motor_hw_info){.timer = &FADER4_TIM_HANDLE, .timer_a_channel = FADER4_A_TIM_AF, .timer_b_channel = FADER4_B_TIM_AF},
};

static void update_motor_power (fader_info_t *info, int32_t power)
{
    const motor_hw_info *hw = &(motor_hw[info->channel]);


    const uint8_t main_channel = power > 0 ? hw->timer_a_channel : hw->timer_b_channel;
    const uint8_t low_channel = power > 0 ? hw->timer_b_channel : hw->timer_a_channel;

    power = power < 0 ? (power == INT32_MIN ? INT32_MAX : -power) : power;

    __HAL_TIM_SET_COMPARE(hw->timer, main_channel, power);
    __HAL_TIM_SET_COMPARE(hw->timer, low_channel, 0);
}

/**
 * @brief Feedback control based on motor target and position
 * Feedback control to move the motor towards a target position;
 * currently just uses proportional deviation with no derivative or integral
 */
static int32_t get_motor_power (fader_info_t *info, fader_state_t *state)
{
    const int32_t where_it_is = get_physical_fader_position(info);
    const int32_t where_it_isnt = state->position;

    int64_t deviation = where_it_isnt - where_it_is;
    if (deviation < -FADER_DEVIATION_UNTIL_MOTOR_MAX)
    {
        deviation = -FADER_DEVIATION_UNTIL_MOTOR_MAX;
    }
    else if (deviation > FADER_DEVIATION_UNTIL_MOTOR_MAX)
    {
        deviation = FADER_DEVIATION_UNTIL_MOTOR_MAX;
    }

    const uint32_t motor_pwm_period = __HAL_TIM_GET_AUTORELOAD(motor_hw[info->channel].timer) + 1;
    return (deviation * motor_pwm_period) / FADER_DEVIATION_UNTIL_MOTOR_MAX;
}

void update_fader (fader_info_t *info, fader_state_t *old_state, fader_state_t *new_state)
{
    fader_state_t new_state_i = *old_state;
    if (is_fader_touched(info))
    {
        new_state_i.movement_mode = FADER_UNPOWERED;
        new_state_i.position = get_physical_fader_position(info);
    }
    else
    {
        new_state_i.movement_mode = FADER_MOVE_OR_HOLD_TARGET;
        update_motor_power(info, get_motor_power(info, &new_state_i));
    }

    *new_state = new_state_i;
}
