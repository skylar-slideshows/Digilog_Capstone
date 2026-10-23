#ifndef FADER_DRIVER_H
#define FADER_DRIVER_H

#include <stdint.h>
#include "stm32g474xx.h"
#include "stm32g4xx_ll_adc.h"

#define DEFAULT_FADER_STATE (fader_state_t){.movement_mode = FADER_UNPOWERED, .position = UINT16_MAX >> 5}

/**
 * @brief Hardware connection info of the fader
 */
typedef struct
{
    GPIO_TypeDef *touch_sensor_port;
    uint16_t touch_sensor_pin;

    GPIO_TypeDef *motor_a_port;
    uint16_t motor_a_pin;
    GPIO_TypeDef *motor_b_port;
    uint16_t motor_b_pin;

    uint8_t channel; //!< Channel in the bucket
} fader_info_t;

typedef enum
{
    FADER_UNPOWERED,           //!< Allow free motion to drive position (active when touch detected)
    FADER_MOVE_OR_HOLD_TARGET, //!< Move to or hold a driven position
} fader_movement_mode_state;

typedef struct
{
    fader_movement_mode_state movement_mode;
    uint16_t position;
} fader_state_t;

// TODO: maybe add calibration options to state (i.e lower and upper values)

/**
 * @brief Runs one update of a fader's physical behavior using its cached state: reads the physical position
 * while it is touched, or drives the motor toward the target position otherwise.
 * The driver caches the state of each fader, so call this exactly once per update cycle per fader.
 */
void update_fader (
    const fader_info_t *info //!< Hardware connection info of the fader
);

/**
 * @brief Get a snapshot of a fader's cached state. While the mode is FADER_UNPOWERED, position is the physical position;
 * otherwise it is the target position.
 */
fader_state_t get_fader_state (const fader_info_t *info);

/**
 * @brief Set the position the fader will move to and hold when it is not touched
 */
void set_fader_target (const fader_info_t *info, uint16_t position);

/**
 * @brief Poll fader values into the cache/buffer
 */
void poll_faders(void);

/**
 * @brief Init fader timers and adc stuff
 */
void init_faders(void);

#endif
