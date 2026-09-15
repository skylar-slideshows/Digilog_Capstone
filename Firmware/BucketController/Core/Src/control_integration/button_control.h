#ifndef BUTTON_CONTROL_H
#define BUTTON_CONTROL_H

/**
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

#include "control_integration/control_interface.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"

void init_button_controls(uint8_t channel, channel_control_io_state *state, channel_control_io_t *io);
void update_channel_button_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io);
void update_channel_button_leds (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io);

#endif
