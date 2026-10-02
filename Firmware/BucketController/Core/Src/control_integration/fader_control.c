#include "fader_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/fader_driver.h"
#include "hardware_state_sets.h"
#include "hardware_sets.h"
#include "main.h"
#include "portmacro.h"
#include <stdint.h>

static inline s_scalar_control_t uint16_to_s_scalar (uint16_t in)
{
    return (s_scalar_control_t)(in ^ 0x8000); // XOR MSB
}
static inline uint16_t s_scalar_to_uint16 (s_scalar_control_t in)
{
    return (uint16_t)(in ^ 0x8000); // XOR MSB
}

void update_channel_fader_hardware (channel_control_io_state *state, channel_control_io_t *io)
{
    update_fader(&(io->fader), &(state->fader_state), &(state->fader_state));
}

void update_channel_fader_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_state *state,
    channel_control_io_t *io
)
{
    // TODO: Try to find a way to be more conservative about taking the mutex (only take it if values have changed?)
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    if (state->fader_state.movement_mode == FADER_UNPOWERED)
    {
        vals->output_gain = uint16_to_s_scalar(state->fader_state.position);
    }
    else
    {
        state->fader_state.position = s_scalar_to_uint16(vals->output_gain);
    }
    xSemaphoreGive(*vals_mutex);
}

void init_fader_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{
    switch (channel)
    {
    case 0:
        io->fader = (fader_info_t){.touch_sensor_port = F0_Touch_GPIO_Port,
                                   .touch_sensor_pin = F0_Touch_Pin,
                                   .motor_a_port = F0_MA_GPIO_Port,
                                   .motor_a_pin = F0_MA_Pin,
                                   .motor_b_port = F0_MB_GPIO_Port,
                                   .motor_b_pin = F0_MB_Pin,
                                   .channel = channel};
        break;
    case 1:
        io->fader = (fader_info_t){.touch_sensor_port = F1_Touch_GPIO_Port,
                                   .touch_sensor_pin = F1_Touch_Pin,
                                   .motor_a_port = F1_MA_GPIO_Port,
                                   .motor_a_pin = F1_MA_Pin,
                                   .motor_b_port = F1_MB_GPIO_Port,
                                   .motor_b_pin = F1_MB_Pin,
                                   .channel = channel};
        break;
    case 2:
        io->fader = (fader_info_t){.touch_sensor_port = F2_Touch_GPIO_Port,
                                   .touch_sensor_pin = F2_Touch_Pin,
                                   .motor_a_port = F2_MA_GPIO_Port,
                                   .motor_a_pin = F2_MA_Pin,
                                   .motor_b_port = F2_MB_GPIO_Port,
                                   .motor_b_pin = F2_MB_Pin,
                                   .channel = channel};
        break;
    case 3:
        io->fader = (fader_info_t){.touch_sensor_port = F3_Touch_GPIO_Port,
                                   .touch_sensor_pin = F3_Touch_Pin,
                                   .motor_a_port = F3_MA_GPIO_Port,
                                   .motor_a_pin = F3_MA_Pin,
                                   .motor_b_port = F3_MB_GPIO_Port,
                                   .motor_b_pin = F3_MB_Pin,
                                   .channel = channel};
        break;
    }

    state->fader_state = DEFAULT_FADER_STATE;
}
