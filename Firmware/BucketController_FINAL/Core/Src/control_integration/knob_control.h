#ifndef ROTARY_ENCODER_CONTROL_H
#define ROTARY_ENCODER_CONTROL_H

#include "control_integration/control_interface.h"
#include "hardware_sets.h"

/*
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

void init_knob_controls (uint8_t channel, channel_control_io_t *io);

/** Flashes every EQ ring on a channel a few times (after the EQ has been reset). */
void knob_flash_eq_rings (uint8_t channel);

/** Flashes every sends ring on a channel a few times (after the sends have been reset). */
void knob_flash_send_rings (uint8_t channel);

/** Flashes the six compressor rings on a channel a few times (after the compressor has been reset). */
void knob_flash_comp_rings (uint8_t channel);
void update_channel_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
);
void update_channel_knob_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
);

#endif
