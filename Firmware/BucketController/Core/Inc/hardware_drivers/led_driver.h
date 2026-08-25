/**
  **********************************************************************************
  * LED DISPLAYS DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file led_driver.h
  * @brief Skylar's driver for the LED displays on the console, including knobs and buttons.
  *        CONFIGURATION in top of .c file.
  *
  * Low level driver for showing stuff on the LEDs.
  * Includes all basic writing functions and center/from-left writing for knob
  * rings and both (bar/point) display style modes. There are 40, 32 LED knob rings
  * for a bucket controller, and (up to) 64 more bits for 16 button LEDs per
  * channel (Total 1344 LEDs per bucket). Part: 74HCT595.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-08-20
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026 Skylar Denno
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the “Software”), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>
#include <stdbool.h>


/**
 ----------------------------------------------------------------------------------
  @brief Display mode (cosmetic): 0 = bar / line of lights, 1 = point indicator
 ----------------------------------------------------------------------------------
*/
typedef enum {
    DISP_BAR = 0,
    DISP_POINT = 1
} knob_disp_t;


/**
 ----------------------------------------------------------------------------------
  @brief Knob scale modes: 0 = standard left to right, 1 = Dist. from center (pan knob, gain/atten)
 ----------------------------------------------------------------------------------
*/
typedef enum {
    SCALE_LEFT = 0, // 0-32 bar grows from the left
    SCALE_CENTER = 1  // -15 - +16, bar grows from the middle LED (0)
} knob_scale_t;


/**
 ----------------------------------------------------------------------------------
  @brief GPIO, OE PWM setup, turn off all LEDs
 ----------------------------------------------------------------------------------
*/
void led_init(void);


/**
 ----------------------------------------------------------------------------------
  @brief Push frame to hardware IF changed
 ----------------------------------------------------------------------------------
*/
void led_update(void);


/**
 ----------------------------------------------------------------------------------
  @brief Push frame to hardware UNCONDITIONALLY
 ----------------------------------------------------------------------------------
*/
void led_refresh(void);


/**
 ----------------------------------------------------------------------------------
  @brief Clear all LED states (in the data, not with OE pin), turning them all off
 ----------------------------------------------------------------------------------
*/
void led_clear(void);


/**
 ----------------------------------------------------------------------------------
  @brief Set brightness of all LEDs through OE pin PWM duty cycle (0 to 255)
  @param brightness Brightness 0 - 255 integer.
 ----------------------------------------------------------------------------------
*/
void led_brightness(uint8_t brightness);


/**
 ----------------------------------------------------------------------------------
  @brief Gives the current brightness setting 0 - 255
 ----------------------------------------------------------------------------------
*/
uint8_t led_get_brightness(void);


/**
 ----------------------------------------------------------------------------------
  @brief Change a knob display's value, specify which knob by coords (chnl and knob number)
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @param value int between 0-32 for left/standard mode and -15 to 16 for center/pan mode (0 = center)
 ----------------------------------------------------------------------------------
*/
void knob_led(uint8_t channel, uint8_t knob, int8_t value);


/**
 ----------------------------------------------------------------------------------
  @brief Changes a knob display by delta LEDs from its current value, clamped to its scale.
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @param delta int change by this value
 ----------------------------------------------------------------------------------
*/
void knob_step(uint8_t channel, uint8_t knob, int8_t delta);


/**
 ----------------------------------------------------------------------------------
  @brief Sets whether a knob draws as a filled bar or a single point.
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @param mode display mode (0 = bar, 1 = point, see knob_disp_t)
 ----------------------------------------------------------------------------------
*/
void knob_disp(uint8_t channel, uint8_t knob, knob_disp_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief Sets a knob to the left (0..32) or centre (-15..16) scale, converting
         its stored value so the ring does not jump.
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @param mode scale mode (0 = from left, 1 = center/pan, see knob_scale_t)
 ----------------------------------------------------------------------------------
*/
void knob_scale(uint8_t channel, uint8_t knob, knob_scale_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief Returns a knob's current value, in the units of its own scale.
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @return returns the value of the knob display (-15 to 16 or 0 to 32 depending on scale mode)
 ----------------------------------------------------------------------------------
*/
int8_t knob_get(uint8_t channel, uint8_t knob);


/**
 ----------------------------------------------------------------------------------
  @brief Returns a knob's current display mode (bar or point).
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @return returns a knob display mode 0 or 1 (see knob_disp_t)
 ----------------------------------------------------------------------------------
*/
knob_disp_t knob_get_disp(uint8_t channel, uint8_t knob);


/**
 ----------------------------------------------------------------------------------
  @brief Returns a knob's current scale mode (left or center).
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @return returns a knob scale mode 0 or 1 (see knob_scale_t)
 ----------------------------------------------------------------------------------
*/
knob_scale_t knob_get_scale(uint8_t channel, uint8_t knob);


/**
 ----------------------------------------------------------------------------------
  @brief Lights an arbitrary ring pattern, bit31 = LED 0 (left), bypassing the
         knob's stored value and modes.
  @param channel channel number 0-3
  @param knob knob inside that channel number 0-9
  @param bits 32 bits corresponding to LEDs left to right, 1 = on
 ----------------------------------------------------------------------------------
*/
void knob_raw(uint8_t channel, uint8_t knob, uint32_t bits);


#define knob_inc(channel, knob) knob_step((channel), (knob), +1)
#define knob_dec(channel, knob) knob_step((channel), (knob), -1)


/**
 ----------------------------------------------------------------------------------
  @brief Set a specific button display LED to on or off by coordinate
  @param channel channel number 0-3
  @param button channel's button number 0-15
  @param on set that button LED on or off? true = on
 ----------------------------------------------------------------------------------
*/
void button_led(uint8_t channel, uint8_t button, bool on);


/**
 ----------------------------------------------------------------------------------
  @brief Returns if specified button LED is on or off
  @param channel channel number 0-3
  @param button channel's button number 0-15
  @return boolean value of that button's LED display on/off
 ----------------------------------------------------------------------------------
*/
bool button_get(uint8_t channel, uint8_t button);


/**
 ----------------------------------------------------------------------------------
  @brief Sets 16 button LED states (entire channel) at once in the frame buffer
  @param channel channel number 0-3
  @param mask 16-bits representing that channel's 16 button displays, 1 = on
 ----------------------------------------------------------------------------------
*/
void button_mask(uint8_t channel, uint16_t mask);


/**
 ----------------------------------------------------------------------------------
  @brief type to store animation
 ----------------------------------------------------------------------------------
*/
typedef enum {
    ANIM_NONE = 0, ANIM_LOAD, ANIM_SWEEP, ANIM_BREATHE_KEEP, ANIM_BREATHE_ALL
} anim_t;


/**
 ----------------------------------------------------------------------------------
  @brief anim_stop : Redraws every knob from its stored value, restores the saved brightness and
  resets all animation phases
 ----------------------------------------------------------------------------------
*/
void anim_stop(void);


/**
 ----------------------------------------------------------------------------------
  @brief anim_claim : stop what was running and claim display
  @param a anim_t "animation type" type, changes what it does a bit
 ----------------------------------------------------------------------------------
*/
void anim_claim(anim_t a);


/**
 ----------------------------------------------------------------------------------
  @brief anim_loading : Knob 0-9 or -1 for all (int8) -> Void
  Spinning loading wheel, call repeatedly, one call = one advancement.
  When loading done just stop and call anim_stop.
  @param knob knob number 0-9 (all channels that knob will do loading), use -1 to do every knob
 ----------------------------------------------------------------------------------
*/
void anim_loading(int8_t knob);

/**
 ----------------------------------------------------------------------------------
  @brief anim_sweep : Channel 0-3 (uint8), Knob 0-9 (uint8) -> Void
  One step of a back-and-forth sweep, for knobs in left to right mode, and knobs in
  center mode the animation matches the mode.
  @param channel channel 0-3
  @param knob knob 0-9
 ----------------------------------------------------------------------------------
*/
void anim_sweep(uint8_t channel, uint8_t knob);


/**
 ----------------------------------------------------------------------------------
  @brief anim_breathe : Brightness breathing animation
  @param mode mode 0 = just the LEDs that
  are currently on in normal state will do it, mode 1 = every single LED does it
 ----------------------------------------------------------------------------------
*/
void anim_breathe(uint8_t mode);


/**
 ----------------------------------------------------------------------------------
  @brief [DEBUG MODE] Toggles debug mode with input boolean
 ----------------------------------------------------------------------------------
*/
void led_debug(bool on);


/**
 ----------------------------------------------------------------------------------
  @brief [DEBUG MODE] Prints the configuration params to console
 ----------------------------------------------------------------------------------
*/
void led_print_config(void);

#endif
