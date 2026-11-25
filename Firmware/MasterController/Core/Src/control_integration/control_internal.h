#ifndef CONTROL_INTERNAL_H
#define CONTROL_INTERNAL_H

/**
 * Internal to control_integration: what knob_control.c and button_control.c report back to
 * control_interface.c. Not a public header.
 */

#include <stdint.h>
#include "control_integration/control_interface.h"

/** Something on the panel changed a value (for control_last_touched) */
void control_touched (touched_param_t param, uint8_t which);

/** The screen's controls: K4 turned (detents, + clockwise), K4 pushed, B0-B3 pressed */
void screen_input_turned (int8_t steps);
void screen_input_knob_pressed (void);
void screen_input_button_pressed (uint8_t button);

/** What the screen asked of B0-B3's lights (control_set_screen_leds) */
void screen_leds_get (uint8_t *on_mask, uint8_t *flash_mask);

#endif
