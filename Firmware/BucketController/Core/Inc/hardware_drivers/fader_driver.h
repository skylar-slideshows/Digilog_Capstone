#ifndef FADER_DRIVER_H
#define FADER_DRIVER_H

#include <stdint.h>

#define DEFAULT_FADER_STATE (fader_state_t)({.movement_mode = FADER_UNPOWERED, .position = UINT16_MAX / 2})

//! Hardware connection info of the fader
typedef struct
{
    // TODO / PLACEHOLDER
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
 * @brief Updates a fader state based on physical behavior, or a fader's physical behavior based on its state
 */
void update_fader (
    fader_info_t *info,       //!< Hardware connection info of the fader
    fader_state_t *old_state, //!< Pointer to the old fader state containing its last polled values
    fader_state_t *new_state  //!< Pointer in which to store the new fader state
);

#endif
