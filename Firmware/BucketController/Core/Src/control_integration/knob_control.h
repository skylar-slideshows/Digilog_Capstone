#ifndef ROTARY_ENCODER_CONTROL_H
#define ROTARY_ENCODER_CONTROL_H

#include "control_integration/control_interface.h"
#include "hardware_sets.h"
#include "hardware_state_sets.h"

/**
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

void init_knob_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io);
void update_channel_knob_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io);
void update_channel_knob_leds (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io);

#endif
