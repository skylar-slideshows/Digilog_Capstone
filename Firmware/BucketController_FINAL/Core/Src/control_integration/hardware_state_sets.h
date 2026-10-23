#ifndef HARDWARE_STATE_SETS_H
#define HARDWARE_STATE_SETS_H

#include "hardware_drivers/fader_driver.h"
#include "control_integration/control_interface.h"

/**
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 *
 * Per-device state of knobs and buttons (previous pin states, press timing) is cached in their drivers.
 */

/**
 * @brief struct containing the control-level state of one channel's inputs
 */
typedef struct
{
    channel_control_set_selection_t control_set_selection; //!< SENDS/EQ/COMP control type

    fader_state_t fader_state;

} channel_control_io_state;

#endif
