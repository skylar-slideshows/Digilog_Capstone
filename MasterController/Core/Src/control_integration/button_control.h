#ifndef BUTTON_CONTROL_H
#define BUTTON_CONTROL_H

/**
 * This header file is internal to control_integration, separated for organization (as on the bucket).
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

#include "control_integration/control_interface.h"
#include "hardware_sets.h"

void init_button_controls (master_control_io_t *io);

void update_button_values (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io);

void update_button_leds (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io);

#endif
