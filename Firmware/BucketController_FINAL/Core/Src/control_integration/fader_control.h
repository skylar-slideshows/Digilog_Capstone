#ifndef FADER_CONTROL_H
#define FADER_CONTROL_H

#include "hardware_sets.h"

/*
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

void init_fader_controls (uint8_t channel, channel_control_io_t *io);
void update_channel_fader_hardware (channel_control_io_t *io);
void update_channel_fader_values (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io);

#endif
