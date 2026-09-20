/**
  **********************************************************************************
  * LED DISPLAYS DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file hardware_drivers/led_driver.h
  * @brief Shift register based LED display driver, using LED devices with arbitrary width.
  * This driver contains all fn's for rendering values and animations into bits
  * and serializes them for output.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-09-18
  * @version 2.2
  *
  * @attention
  *  Copyright (C) 2026 Skylar Denno
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the "Software"), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.h"


/*=============================== CHAIN SIZE ================================*/
// Both of these may be overridden in CONFIG.h. LED_CHAIN_BITS must be >= the
// sum of all registered device widths

#ifndef LED_CHAIN_BITS
#define LED_CHAIN_BITS (CHANNELS * (KNOBS_PER_CHAN * LEDS_PER_KNOB + BUTTONS_PER_CHAN))
#endif

#ifndef LED_MAX_DEVICES
#define LED_MAX_DEVICES (CHANNELS * (KNOBS_PER_CHAN + BUTTONS_PER_CHAN) + 8)
#endif

#define LED_FRAME_BYTES ((LED_CHAIN_BITS + 7) / 8)


/*=============================== DATA TYPES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Display mode (cosmetic): 0 = bar / line of lights, or 1 = single point indicator
 ----------------------------------------------------------------------------------
*/
typedef enum
{
    DISP_BAR   = 0,
    DISP_POINT = 1
} knob_disp_t;


/**
 ----------------------------------------------------------------------------------
  @brief Scale mode: is a knob ring displaying an unsigned (from left = 0) or 1 = signed / from center value?
 ----------------------------------------------------------------------------------
*/
typedef enum
{
    SCALE_LEFT   = 0,
    SCALE_CENTER = 1
} knob_scale_t;


/**
 ----------------------------------------------------------------------------------
  @brief One physical display element on the cascade.
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    uint16_t value;  // raw control value to show 0-65535
    knob_scale_t scale; // scale mode (from left or from center)
    knob_disp_t disp_mode; // display mode (filled or point)

    uint8_t width; // number of leds belonging to the device 
    uint16_t offset; // bit index of led 0
    uint8_t center; // what LED is its center LED?
    bool    dirty; // does the value not match the latched/displayed LEDs?
    bool    raw;   // renderer suspended; led_raw owns these bits
    
} led_device_t;


/**
 ----------------------------------------------------------------------------------
  @brief Contains the setup args for led_add.
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    knob_scale_t scale; // scale mode (from left or from center)
    uint8_t width; // number of leds belonging to the device 
} led_device_init_t;


/*=============================== INIT ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Initializes everything
 ----------------------------------------------------------------------------------
*/
void led_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief Append one device to the cascade (list of devices). Call order IS cascade order.
 ----------------------------------------------------------------------------------
*/
void led_add (uint8_t width, knob_scale_t scale);


/**
 ----------------------------------------------------------------------------------
  @brief Get device at registration index
 ----------------------------------------------------------------------------------
*/
led_device_t *led_device_at (uint8_t idx);


/*=============================== VALUES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Set an unsigned control value. 0 = empty, 65535 = full.
 ----------------------------------------------------------------------------------
*/
void led_set (uint8_t device_idx, uint16_t value);


/**
 ----------------------------------------------------------------------------------
  @brief Set a signed control value for a SCALE_CENTER device. 0 = center.
 ----------------------------------------------------------------------------------
*/
void led_set_signed (uint8_t device_idx, int16_t value);


/**
 ----------------------------------------------------------------------------------
  @brief Set an on/off device (buttons).
 ----------------------------------------------------------------------------------
*/
void led_set_bool (uint8_t device_idx, bool on);


/**
 ----------------------------------------------------------------------------------
  @brief Get the current raw value displayed by a device
 ----------------------------------------------------------------------------------
*/
uint16_t led_get (led_device_t *device);


/*=============================== RENDER ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Renderer (control value to display -> LED serial bits to display) for knobs (Width > 1)
 ----------------------------------------------------------------------------------
*/
void led_render (led_device_t *device);


/**
 ----------------------------------------------------------------------------------
  @brief COMMONLY CALLED!! Full update cycle: renders all updated values into bits for LED
  display, serializes them and shifts them out to the shift registers, and latches them.
 ----------------------------------------------------------------------------------
*/
void led_update (void);


/**
 ----------------------------------------------------------------------------------
  @brief Turn off all LEDs by setting their values to 0 (danger!)
 ----------------------------------------------------------------------------------
*/
void led_clear (void);


/*=============================== BRIGHTNESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Set brightness of knob leds (0-255)
 ----------------------------------------------------------------------------------
*/
void led_brightness (uint8_t brightness);


/**
 ----------------------------------------------------------------------------------
  @brief What is current brightness
 ----------------------------------------------------------------------------------
*/
uint8_t led_get_brightness (void);


/*=============================== SETTING MODES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Sets display mode (0 = bar, fill range of LEDs; 1 = point)
 ----------------------------------------------------------------------------------
*/
void led_set_disp (led_device_t *device, knob_disp_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief Sets scale mode (0 = unsigned from left knobs, 1 = signed from center)
 ----------------------------------------------------------------------------------
*/
void led_set_scale (led_device_t *device, knob_scale_t mode);


/*=============================== FILLING AND CLEARING ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Turns off all LEDs in a span
 ----------------------------------------------------------------------------------
*/
void led_device_clear (led_device_t *device);

/**
 ----------------------------------------------------------------------------------
  @brief Set any specific LED inside a device's span to a value.
 ----------------------------------------------------------------------------------
*/
void led_specific_set (led_device_t *device, uint8_t i, bool on);

/**
 ----------------------------------------------------------------------------------
  @brief Turn on LEDs [Lo...Hi] inclusive of endpoints. Stays in the span though.
  (Won't bleed over to other devices in the chain if indexes are bad)
 ----------------------------------------------------------------------------------
*/
void led_span_fill (led_device_t *device, uint8_t lo, uint8_t hi);


/*=============================== RAW ACCESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Light an arbitrary pattern, bit 0 of bits = LED 0. Suspends the renderer
  for this device (value is kept) until led_release.
 ----------------------------------------------------------------------------------
*/
void led_raw (led_device_t *device, uint32_t bits);


/**
 ----------------------------------------------------------------------------------
  @brief Hand the device back to its renderer and redraw it from its stored value.
 ----------------------------------------------------------------------------------
*/
void led_release (led_device_t *device);


/**
 ----------------------------------------------------------------------------------
  @brief Dev mode thing
 ----------------------------------------------------------------------------------
*/
void led_print_config (void);


/*=============================== ANIMATIONS ================================*/
// These are frame refreshers: call repeatedly, one call = one step. They write
// through led_raw / the OE duty and never disturb stored values.

typedef enum
{
    ANIM_NONE = 0,
    ANIM_LOAD,
    ANIM_SWEEP,
    ANIM_BREATHE_KEEP,
    ANIM_BREATHE_ALL
} anim_t;


/**
 ----------------------------------------------------------------------------------
  @brief anim stop : Run this after animation to return to regular LED operation
 ----------------------------------------------------------------------------------
*/
void anim_stop (void);


/**
 ----------------------------------------------------------------------------------
  @brief anim_loading : Spinning loading animation - call repeatedly
 ----------------------------------------------------------------------------------
*/
void anim_loading (void);


/**
 ----------------------------------------------------------------------------------
  @brief anim_sweep : Animate up and down sweep of LEDs - call repeatedly
 ----------------------------------------------------------------------------------
*/
void anim_sweep (led_device_t *device, uint16_t phase);


/**
 ----------------------------------------------------------------------------------
  @brief anim_breathe : Brightness up and down animation - call repeatedly
 ----------------------------------------------------------------------------------
*/
void anim_breathe (uint8_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief led_loading : turn on and off loading animation!
 ----------------------------------------------------------------------------------
*/
void led_loading (bool on);

#endif