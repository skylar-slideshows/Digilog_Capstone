#ifndef ROTARY_ENCODER_CONTROL_H
#define ROTARY_ENCODER_CONTROL_H

#include "control_integration/control_interface.h"
#include "hardware_sets.h"

/*
 * This header file is internal to control_integration, separated for organization (as on the bucket).
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

void init_knob_controls (master_control_io_t *io);

void update_knob_values (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io);

void update_knob_leds (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io);

#endif
