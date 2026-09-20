#ifndef HARDWARE_STRUCTS_H
#define HARDWARE_STRUCTS_H

/**
 * This header file is internal to control_integration, separated for organization.
 * It should not have to be included anywhere outside of control_integration; it is not a public header
 */

/*=============================== INDIVIDUAL HARDWARE DATA STRUCTURES ================================*/

#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"

/**
 * @brief Info on the location of a single button LED
 */
typedef struct
{
    // TODO
} button_led_info_t;


/**
 * @brief Info on the rotary encoder and led ring of one knob
 */
typedef struct
{
    encoder_info_t encoder;
    led_device_t *led_ring;
} knob_info_t;

/**
 * @brief Info on the button and led of a lit button
 */
typedef struct
{
    button_info_t button;
    button_led_info_t led;
} lit_button_info_t;

#endif
