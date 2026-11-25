#include "fader_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/fader_driver.h"
#include "hardware_sets.h"
#include "CONFIG.h"
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

extern channel_control_io_t channel_controls_io[CHANNELS];

bool fader_control_touched (uint8_t channel)
{
#if FADERS_CONNECTED
    if (channel >= CHANNELS) return false;
    return get_fader_state(&(channel_controls_io[channel].fader)).movement_mode == FADER_UNPOWERED;
#else
    (void)channel;
    return false;
#endif
}

void update_channel_fader_hardware (channel_control_io_t *io)
{
#if FADERS_CONNECTED
    update_fader(&(io->fader));
#else
    (void)io; // faders unplugged: motors left off (CONFIG.h)
#endif
}

void update_channel_fader_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
#if !FADERS_CONNECTED
    // faders unplugged: their open inputs read random positions and touches, so leave the level alone (CONFIG.h)
    (void)vals_mutex;
    (void)vals;
    (void)io;
    return;
#endif
    // TODO: Try to find a way to be more conservative about taking the mutex (only take it if values have changed?)
    const fader_state_t fader = get_fader_state(&(io->fader));

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    if (fader.movement_mode == FADER_UNPOWERED)
    {
        vals->output_gain = uint16_to_s_scalar(fader.position);
    }
    else
    {
        set_fader_target(&(io->fader), s_scalar_to_uint16(vals->output_gain));
    }
    xSemaphoreGive(*vals_mutex);
}

// Hardware connections of each channel's fader (CubeMX labels; timers and ADCs are in CONFIG.h)
static const fader_info_t fader_hw[CHANNELS] = {
    {.touch_sensor_port = F0_TOUCH_GPIO_Port, .touch_sensor_pin = F0_TOUCH_Pin,
     .motor_a_port = F0_MA_GPIO_Port, .motor_a_pin = F0_MA_Pin,
     .motor_b_port = F0_MB_GPIO_Port, .motor_b_pin = F0_MB_Pin},
    {.touch_sensor_port = F1_TOUCH_GPIO_Port, .touch_sensor_pin = F1_TOUCH_Pin,
     .motor_a_port = F1_MA_GPIO_Port, .motor_a_pin = F1_MA_Pin,
     .motor_b_port = F1_MB_GPIO_Port, .motor_b_pin = F1_MB_Pin},
    {.touch_sensor_port = F2_TOUCH_GPIO_Port, .touch_sensor_pin = F2_TOUCH_Pin,
     .motor_a_port = F2_MA_GPIO_Port, .motor_a_pin = F2_MA_Pin,
     .motor_b_port = F2_MB_GPIO_Port, .motor_b_pin = F2_MB_Pin},
    {.touch_sensor_port = F3_TOUCH_GPIO_Port, .touch_sensor_pin = F3_TOUCH_Pin,
     .motor_a_port = F3_MA_GPIO_Port, .motor_a_pin = F3_MA_Pin,
     .motor_b_port = F3_MB_GPIO_Port, .motor_b_pin = F3_MB_Pin},
};

void init_fader_controls (uint8_t channel, channel_control_io_t *io)
{
    if (channel < CHANNELS)
    {
        io->fader = fader_hw[channel];
        io->fader.channel = channel;
    }
}
