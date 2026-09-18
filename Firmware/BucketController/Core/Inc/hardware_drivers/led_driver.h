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
  * @version 2.1
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
// sum of all registered device widths; led_chain_finalize() checks it.

#ifndef LED_CHAIN_BITS
#define LED_CHAIN_BITS (CHANNELS * (KNOBS_PER_CHAN * LEDS_PER_KNOB + BUTTONS_PER_CHAN))
#endif

#ifndef LED_MAX_DEVICES
#define LED_MAX_DEVICES (CHANNELS * (KNOBS_PER_CHAN + BUTTONS_PER_CHAN) + 8)
#endif

#define LED_FRAME_BYTES ((LED_CHAIN_BITS + 7) / 8)


/*=============================== DISPLAY MODES ================================*/

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
  @brief What kind of control this device belongs to. Used only for lookup and for
  aiming animations at a subset of devices; the renderer does the actual drawing.
 ----------------------------------------------------------------------------------
*/
typedef enum
{
    LED_ROLE_NONE = 0,
    LED_ROLE_RING,   //!< knob ring
    LED_ROLE_BUTTON, //!< button backlight
    LED_ROLE_BAR,    //!< fader / meter bar
    LED_ROLE_OTHER
} led_role_t;


/*=============================== SPANS ================================*/
// a span is an led_device's set of LEDs in the frame buffer

typedef struct
{
    uint16_t offset;      // bit index of this device's first LED
    uint16_t width;       // number of LEDs belonging to this device
    uint8_t  reverse: 1;
    uint8_t  invert: 1;   // 1 = common anode, 0 = common cathode
} led_span_t;

/**
 ----------------------------------------------------------------------------------
  @brief Turns off all LEDs in a span
 ----------------------------------------------------------------------------------
*/
void led_span_clear (led_span_t sp);

/**
 ----------------------------------------------------------------------------------
  @brief Set any specific LED inside a span to a value.
 ----------------------------------------------------------------------------------
*/
void led_span_set (led_span_t sp, uint16_t i, bool on);

/**
 ----------------------------------------------------------------------------------
  @brief Turn on LEDs [Lo...Hi] inclusive of endpoints. Stays in the span though.
  (Won't bleed over to other devices in the chain if indexes are bad)
 ----------------------------------------------------------------------------------
*/
void led_span_fill (led_span_t sp, uint16_t lo, uint16_t hi);


/*=============================== DEVICES ================================*/

typedef struct led_device led_device_t;

/**
 ----------------------------------------------------------------------------------
  @brief Turns a device's stored value into bits, drawing through dev->span. Must
  write every LED in the span (i.e. clear what it does not light), and must not
  touch anything outside it.
 ----------------------------------------------------------------------------------
*/
typedef void (*led_render_fn) (const led_device_t *dev);


/**
 ----------------------------------------------------------------------------------
  @brief One physical display element on the cascade.
  Fields are public for renderers to read; write them through the API only.
 ----------------------------------------------------------------------------------
*/
struct led_device
{
    led_render_fn render; // which value -> bits function does this device use? (render for knob, render_boolean for buttons)
    led_span_t    span;   // where this device is on the chain (range of LEDs belonging to it)
    uint16_t      center; // what LED is its center LED?
    uint16_t      value;  // raw control value; signed if scale == SCALE_CENTER

    uint8_t role;    // led_role_t, for lookup / animation targeting
    uint8_t group;   // console channel 0-3 (or whatever grouping you like)
    uint8_t ordinal; // index within (group, role), e.g. knob 0-9

    uint8_t disp : 1;  // knob_disp_t
    uint8_t scale : 1; // knob_scale_t
    uint8_t dirty : 1; // needs re-render before next push
    uint8_t held : 1;  // raw override active, renderer suspended

    uint8_t anim_phase; // scratch for animations
};


// Flags for led_device_cfg_t.flags
#define LED_F_POINT   0x01u // start in DISP_POINT
#define LED_F_CENTER  0x02u // start in SCALE_CENTER
#define LED_F_REVERSE 0x04u // LED 0 is at the far end of this device's bits
#define LED_F_INVERT  0x08u // active-low hardware


/**
 ----------------------------------------------------------------------------------
  @brief Static description of a device, for the board layout table.
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    uint16_t      width;   // bits consumed on the cascade (32 ring, 1 button, ...)
    led_render_fn render;  // NULL selects led_render_meter
    uint8_t       role;    // led_role_t
    uint8_t       group;   // channel
    uint8_t       ordinal; // index within group+role
    uint8_t       flags;   // LED_F_*
    uint16_t      centre;  // 0 selects (width - 1) / 2
} led_device_cfg_t;


/*=============================== BUILT-IN RENDERERS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Renderer (control value to display -> LED serial bits to display) for knobs (Width > 1)
 ----------------------------------------------------------------------------------
*/
void led_render_meter (const led_device_t *dev);

/**
 ----------------------------------------------------------------------------------
  @brief Renderer (control value to display -> LED serial bits to display) for buttons (Width = 1)
 ----------------------------------------------------------------------------------
*/
void led_render_bool (const led_device_t *dev);

/**
 ----------------------------------------------------------------------------------
  @brief Sets unpopulated devices or any device to all off.
 ----------------------------------------------------------------------------------
*/
void led_render_blank (const led_device_t *dev);


/*=============================== CHAIN BUILD ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Initializes everything
 ----------------------------------------------------------------------------------
*/
void led_init (void);

/**
 ----------------------------------------------------------------------------------
  @brief Append one device to the cascade (list of devices). Call order IS cascade order.
  @param cfg description of the device (copied, may be a compound literal)
  @return handle to the device, or NULL if the chain is full
 ----------------------------------------------------------------------------------
*/
led_device_t *led_add (const led_device_cfg_t *cfg);


/**
 ----------------------------------------------------------------------------------
  @brief Close the chain: pad any leftover bits up to a byte boundary with a blank
  device and validate against LED_CHAIN_BITS. Call once after all led_add calls.
  @return false if the registered devices do not fit the configured chain
 ----------------------------------------------------------------------------------
*/
bool led_chain_finalize (void);


/**
 ----------------------------------------------------------------------------------
  @brief How many devices have been registered?
 ----------------------------------------------------------------------------------
*/
uint16_t led_device_count (void);

/**
 ----------------------------------------------------------------------------------
  @brief Get device at registration index
 ----------------------------------------------------------------------------------
*/
led_device_t *led_device_at (uint16_t index);


/**
 ----------------------------------------------------------------------------------
  @brief Read-only view of the frame buffer as last rendered, LED_FRAME_BYTES long.
 ----------------------------------------------------------------------------------
*/
const uint8_t *led_frame_peek (void);


/*=============================== VALUES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Set an unsigned control value. 0 = empty, 65535 = full.
 ----------------------------------------------------------------------------------
*/
void led_set (led_device_t *dev, uint16_t value);

/**
 ----------------------------------------------------------------------------------
  @brief Set a signed control value for a SCALE_CENTER device. 0 = center.
 ----------------------------------------------------------------------------------
*/
void led_set_signed (led_device_t *dev, int16_t value);

/**
 ----------------------------------------------------------------------------------
  @brief Set an on/off device (buttons).
 ----------------------------------------------------------------------------------
*/
void led_set_bool (led_device_t *dev, bool on);


/**
 ----------------------------------------------------------------------------------
  @brief Get the current raw value displayed by a device
 ----------------------------------------------------------------------------------
*/
uint16_t led_get (led_device_t *dev);


/*=============================== MODES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Sets display mode (0 = bar, fill range of LEDs; 1 = point)
 ----------------------------------------------------------------------------------
*/
void led_set_disp (led_device_t *dev, knob_disp_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief Sets scale mode (0 = unsigned from left knobs, 1 = signed from center)
 ----------------------------------------------------------------------------------
*/
void led_set_scale (led_device_t *dev, knob_scale_t mode);


/*=============================== RAW ACCESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief Light an arbitrary pattern, bit 0 of bits = LED 0. Suspends the renderer
  for this device (value is kept) until led_release.
 ----------------------------------------------------------------------------------
*/
void led_raw (led_device_t *dev, uint32_t bits);


/**
 ----------------------------------------------------------------------------------
  @brief Hand the device back to its renderer and redraw it from its stored value.
 ----------------------------------------------------------------------------------
*/
void led_release (led_device_t *dev);


/*=============================== OUTPUT ================================*/

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
void anim_loading (int8_t ordinal);


/**
 ----------------------------------------------------------------------------------
  @brief anim_sweep : Animate up and down sweep of LEDs - call repeatedly
 ----------------------------------------------------------------------------------
*/
void anim_sweep (led_device_t *dev);


/**
 ----------------------------------------------------------------------------------
  @brief anim_breathe : Brightness up and down animation - call repeatedly
 ----------------------------------------------------------------------------------
*/
void anim_breathe (uint8_t mode);

#endif