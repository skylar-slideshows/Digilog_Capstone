#include <stdbool.h>
#include <stdint.h>
#include "hardware_drivers/fader_driver.h"
#include "control_integration/control_interface.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_adc_ex.h"
#include "stm32g4xx_hal_gpio.h"

#define FADER_POWER_COEFFICIENT 4

extern ADC_HandleTypeDef hadc2;
#define FADER_ADC hadc2

const uint8_t bucket_channel_to_adc_channel[] = {5, 6, 8, 9};
uint32_t adc_buf[CHANNELS];

void init_faders (void)
{
    HAL_ADCEx_Calibration_Start(&FADER_ADC, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&FADER_ADC, adc_buf, CHANNELS);
}

static uint16_t get_physical_fader_position (fader_info_t *info) { return adc_buf[info->channel]; }

static bool is_fader_touched (fader_info_t *info)
{
    return HAL_GPIO_ReadPin(info->touch_sensor_port, info->touch_sensor_pin) == GPIO_PIN_RESET;
}

static void update_motor_power (int32_t power)
{
    // TODO
}

static inline int32_t clamp_mult (int32_t a, int32_t b)
{
    int32_t x = a * b;
    if (a != 0 && x / a != b)
    {
        if (a > 0 && b > 0)
        {
            return INT32_MAX;
        }
        if (a < 0 && b < 0)
        {
            return INT32_MAX;
        }
        return INT32_MIN;
    }
    return x;
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

    const int32_t deviation = where_it_isnt - where_it_is;
    return clamp_mult(deviation, FADER_POWER_COEFFICIENT);
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
        update_motor_power(get_motor_power(info, &new_state_i));
    }

    *new_state = new_state_i;
}
