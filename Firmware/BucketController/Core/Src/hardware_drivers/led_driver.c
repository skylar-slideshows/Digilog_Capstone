/**
  **********************************************************************************
  * LED DISPLAYS DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file hardware_drivers/led_driver.c
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

#include <stddef.h>
#include <stdbool.h>

#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/74hc595.h"
#include "stm32g4xx.h"
#include "CONFIG.h"

#if DEVELOPER_MODE
#include <stdio.h>
#define LED_LOG(...) printf(__VA_ARGS__)
#else
#define LED_LOG(...) ((void)0)
#endif


/*=============================== LED DEVICE CHAIN DEFN ================================*/
// THIS IS WHERE THE PHYSICAL GEOMETRY / HARDWARE LAYOUT IS DEFINED IN SOFTWARE
led_device_init_t init_devices[] = {
    {.width = 1, .scale = SCALE_CENTER}, // test button 0
    {.width = 1, .scale = SCALE_CENTER}, // test button 1
    {.width = 1, .scale = SCALE_CENTER}, // test button 2
    {.width = 32, .scale = SCALE_LEFT},  // test knob 0
    {.width = 32, .scale = SCALE_CENTER} // test knob 1

};


/*=============================== STATE ================================*/
// ALL data stored by driver is here

static uint8_t      frame[LED_FRAME_BYTES];   // one bit per LED, in cascade order, ready to shift out
static led_device_t devices[LED_MAX_DEVICES]; // every display element on the chain, in cascade order
static uint16_t     device_count = 0;         // how many of the above are in use
static uint16_t     used_bits    = 0;         // sum of their widths, so the next led_add knows its offset

static uint8_t       bright = 255;
static volatile bool frame_dirty = true; // true if the frame buffer is not what is currently latched

static void oe_duty (uint8_t brightness);


/*=============================== BIT / SPAN PRIMITIVES ================================*/

// set or clear one bit of a buffer, counting from the MSB of byte 0
static inline void buf_bit (uint8_t *buf, uint16_t b, bool on)
{
    const uint8_t mask = (uint8_t)(0x80u >> (b & 7u));
    if (on) buf[b >> 3] |= mask;
    else    buf[b >> 3] &= (uint8_t)~mask;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_specific_set : set LED i of a device, 0..width-1 in LED order.
  This is where wiring quirks are absorbed: reverse flips the order so a ring
  soldered backwards still counts from its own LED 0, and invert handles hardware
  that sinks current, where a lit LED means a low output.
 ----------------------------------------------------------------------------------
*/
void led_specific_set (led_device_t *device, uint8_t i, bool on)
{
    if (i >= device->width) return;
    const uint16_t b = (uint16_t)(device->offset + i);
    buf_bit(frame, b, on);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_device_clear : turn off every LED belonging to a device
 ----------------------------------------------------------------------------------
*/
void led_device_clear (led_device_t *device)
{
    for (uint8_t i = 0; i < device->width; i++)
    {
        led_specific_set(device, i, false);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief led_span_fill : turn on a run of LEDs, lo..hi inclusive
 ----------------------------------------------------------------------------------
*/
void led_span_fill (led_device_t *device, uint8_t lo, uint8_t hi)
{
    if (lo >= device->width) return;
    if (hi >= device->width) hi = (uint8_t)(device->width - 1u);
    for (uint16_t i = lo; i <= hi; i++)
    {
        led_specific_set(device, i, true);
    }
}


/*=============================== RENDERERS ================================*/

// scale a control value into an LED count: value out of full -> 0..span, rounded to nearest
static uint16_t frac (uint32_t value, uint32_t full, uint16_t span)
{
    if (span == 0u) return 0u;
    if (value >= full) return span;
    return (uint16_t)(((value * span) + (full / 2u)) / full);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_render : default renderer, for anything with more than one
  LED. turns the core control value into a set of bits to display the value on LEDs.
  Puts these bits in the frame buffer to appear at the LEDs corresponding to that control
 ----------------------------------------------------------------------------------
*/
void led_render (led_device_t *device)
{

    led_device_clear(device);

    if ((knob_scale_t)device->scale == SCALE_LEFT)
    {
        // value is unsigned: the bar grows from left LED
        const uint16_t n = frac(device->value, UINT16_MAX, device->width);

        if (n == 0U) return; // all off since value is 0!

        if ((knob_disp_t)device->disp_mode == DISP_POINT)
        {
            led_specific_set(device, (uint16_t)(n - 1U), true);
        } else {
            led_span_fill(device, 0U, (uint16_t)(n - 1U));
        }
    } else {
        // value is signed: the bar grows outwards from the center LED, which is unity
        const int32_t  v = (int32_t)(int16_t)device->value;
        const uint16_t c = (device->center < device->width) ? device->center : (uint16_t)(device->width - 1U);

        if (v >= 0)
        {
            const uint16_t n = frac((uint32_t)v, 32767u, (uint16_t)(device->width - 1U - c));
            if ((knob_disp_t)device->disp_mode == DISP_POINT)
            {
                led_specific_set(device, (uint16_t)(c + n), true);
            } else {
                led_span_fill(device, c, (uint16_t)(c + n));
            }
        } else {
            const uint16_t n = frac((uint32_t)(-v), 32768u, c);
            if ((knob_disp_t)device->disp_mode == DISP_POINT)
            {
                led_specific_set(device, (uint16_t)(c - n), true);
            } else {
                led_span_fill(device, (uint16_t)(c - n), c);
            }
        }
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief led_render_bool : renders buttons (width 1)
 ----------------------------------------------------------------------------------
*/
void led_render_bool (led_device_t *device)
{
    if (device->value)
    {
        led_specific_set(device, 0U, 1);
    } else {
        led_device_clear(device);
    }
}


/*=============================== CHAIN BUILD ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_add : append one device to the chain. Call order is cascade order, so
  a device's index in the list is where it sits on the board, and its bit offset is
  just the running total of everything registered before it.
 ----------------------------------------------------------------------------------
*/
void led_add (uint8_t width, knob_scale_t scale)
{

    if (device_count >= LED_MAX_DEVICES)
    {
        LED_LOG("LED ERR: device table full (%u), raise LED_MAX_DEVICES\n", (unsigned)LED_MAX_DEVICES);
    }

    if ((uint32_t)used_bits + width > (uint32_t)LED_CHAIN_BITS)
    {
        LED_LOG("LED ERR: chain overflow at device %u (%u + %u > %u bits)\n",
                (unsigned)device_count, (unsigned)used_bits,
                (unsigned)width, (unsigned)LED_CHAIN_BITS);
    }

    led_device_t *device = &devices[device_count++];

    device->offset    = used_bits;
    device->width     = width;
    device->center    = 17;
    device->value     = 0U; // initial value to display is 0
    device->disp_mode = 0; // default display mode is bar right now
    device->scale     = scale;

    used_bits = (uint16_t)(used_bits + width);
}


/*=============================== VALUES ================================*/


/**
 ----------------------------------------------------------------------------------
  @brief led_set : Set LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set (led_device_t *device, uint16_t value)
{
    if (device == NULL || device->value == value) return;
    device->value = value;
    device->dirty = true;
    frame_dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_signed : Set signed LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_signed (led_device_t *device, int16_t value)
{
    led_set(device, (uint16_t)value);
    device->dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_bool : Set a button LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_bool (led_device_t *device, bool on)
{
    led_set(device, on ? UINT16_MAX : 0U);
    device->dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_get : get the displayed value from any LED device
 ----------------------------------------------------------------------------------
*/
uint16_t led_get (led_device_t *device)
{
    return device->value;
}


/*=============================== SETTING MODES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_set_disp : Set display mode. 0 = bar fill, 1 = point
 ----------------------------------------------------------------------------------
*/
void led_set_disp (led_device_t *device, knob_disp_t mode)
{
    device->disp_mode = mode;
    device->dirty = true;
    frame_dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_scale : Set scale mode. 0 = from left, 1 = signed / from center
 ----------------------------------------------------------------------------------
*/
void led_set_scale (led_device_t *device, knob_scale_t mode)
{
    device->scale = mode;
    device->dirty = true;
    frame_dirty = true;
}


/*=============================== RAW WRITE ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_raw : draw an arbitrary pattern and suspend the renderer for this
  device, so the stored control value survives untouched underneath. Animations use
  this; led_release hands the device back and redraws it from its value.
 ----------------------------------------------------------------------------------
*/
void led_raw (led_device_t *device, uint32_t bits)
{
    for (uint8_t i = 0; i < device->width; i++)
    {
        led_specific_set(device, i, (bits >> i) & 1U);
    }
    device->dirty = true;
    frame_dirty = true;
}


/*=============================== OUTPUT ================================*/


/**
 ----------------------------------------------------------------------------------
  @brief render_dirty : redraw only the devices who's values have changed
 ----------------------------------------------------------------------------------
*/
static void render_dirty (void)
{
    for (uint16_t i = 0; i < device_count; i++)
    {
        led_device_t *d = &devices[i];
        if (!d->dirty) continue; // skips devices whos values have not changed

        if (d->width == 1) // device is a button led / boolean led
        {
            led_render_bool(d);
        } else {
            led_render(d);
        }
        d->dirty = false; // that device is now clean yay
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief frame_out : shift the whole frame out (serial data), then latch.
 ----------------------------------------------------------------------------------
*/
static void frame_out (void)
{
    uint16_t n = LED_FRAME_BYTES;
    while (n--)
    {
        const uint8_t b = frame[n];
        for (int8_t i = 0; i < 8; i++) shift_bit((uint32_t)((b >> i) & 1U), 1);
    }
    latch_out(1);
    frame_dirty = 0;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_update : important!! render values, push the data out, and latch it
 ----------------------------------------------------------------------------------
*/
void led_update (void)
{
    if (!frame_dirty) return;
    render_dirty();
    frame_dirty = false;
    frame_out();
}


/**
 ----------------------------------------------------------------------------------
  @brief led_clear : turn off all LEDs
 ----------------------------------------------------------------------------------
*/
void led_clear (void)
{
    for (uint16_t i = 0; i < LED_FRAME_BYTES; i++) frame[i] = 0x00;
    frame_dirty = false;
    frame_out();
}


/*=============================== BRIGHTNESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief oe_duty : brightness 0-255 -> PWM duty cycle. CCR2 holds the number of
  CPU cycles per PWM cycle to keep the OE pin high, and OE high blanks the display,
  so a higher CCR2 is dimmer.
 ----------------------------------------------------------------------------------
*/
static void oe_duty (uint8_t brightness)
{
    LED_OE_TIM->CCR2 = ((uint32_t)(255U - brightness) * (LED_OE_TIM->ARR + 1U)) / 255U;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_brightness : sets the knob ring LED brightness (0-255)
 ----------------------------------------------------------------------------------
*/
void led_brightness (uint8_t brightness)
{
    bright = brightness;
    oe_duty(brightness);
}


/*=============================== INIT ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_init : initializes this driver and all LED relateed pins and chip hardware fn's
 ----------------------------------------------------------------------------------
*/
void led_init (void)
{
    device_count = 0;
    used_bits    = 0;

    led_shiftreg_init();
    led_clear();

    /* Brightness is OE pin PWM on timer 1 channel 2. */

    // prescale 0, so the counter runs at the full 170MHz CPU clock
    LED_OE_TIM->PSC = 0;

    // auto reload: 170MHz / 24kHz = 7,083 CPU cycles per PWM cycle
    LED_OE_TIM->ARR = ((uint64_t)CPU_HZ / (uint64_t)BRIGHTNESS_PWM_HZ) - 1U;

    // capture/compare mode: 110 at bit[14:12] is PWM mode 1 (high while count < CCR2),
    // and bit[11] enables the preload register, which holds a mid-cycle change until
    // the next cycle instead of corrupting the current waveform
    LED_OE_TIM->CCMR1 = (6U << 12) | (1U << 11);

    // capture/compare enable: route the compare output to pin OC2
    LED_OE_TIM->CCER = (1U << 4);

    // start blanked (PWM always high = 0 brightness) until led_brightness runs
    LED_OE_TIM->CCR2 = LED_OE_TIM->ARR + 1U;

    // ARPE, so a brightness change mid-cycle cannot produce a bright flash
    LED_OE_TIM->CR1 |= (1U << 7);

    // force an update event so the new ARR and CCR2 take effect, then clear the flag it sets
    LED_OE_TIM->EGR = 1U;
    LED_OE_TIM->SR  = 0;

    // main output enable, the master switch for the PWM pin
    LED_OE_TIM->BDTR |= (1U << 15);

    // start the counter
    LED_OE_TIM->CR1 |= 1U;

    // hand the OE pin over to the timer (alternate function, not plain GPIO)
    LED_OE_PORT->AFR[LED_OE_PIN >> 3] =
        (LED_OE_PORT->AFR[LED_OE_PIN >> 3] & ~(0xFU << ((LED_OE_PIN & 7) * 4)))
      | (LED_OE_TIM_AF << ((LED_OE_PIN & 7) * 4));
    LED_OE_PORT->MODER = (LED_OE_PORT->MODER & ~(3U << (LED_OE_PIN * 2))) | (2U << (LED_OE_PIN * 2));

    led_brightness(bright);

    // CREATE LED DEVICES FOR EVERY DEVICE DEFINED IN HEADER

    for (uint8_t i = 0; i < sizeof(init_devices)/sizeof(init_devices[0]); i++)
    {
        led_add(init_devices[i].width, init_devices[i].scale);
    }

    
}


/**
 ----------------------------------------------------------------------------------
  @brief led_print_config : dev mode stuff
 ----------------------------------------------------------------------------------
*/
#if DEVELOPER_MODE
void led_print_config (void)
{
    printf("\r\nLED chain: %u devices, %u of %u bits, %u bytes",
           (unsigned)device_count, (unsigned)used_bits,
           (unsigned)LED_CHAIN_BITS, (unsigned)LED_FRAME_BYTES);
    printf("\r\n  SER=P%c%u SRCLK=P%c%u RCLK=P%c%u OE=P%c%u",
           'B', LED_SER_PIN, 'B', LED_SRCLK_PIN, 'B', LED_RCLK_PIN, 'C', LED_OE_PIN);
    printf("\r\n  bit clock %lu Hz, frame %lu us",
           (unsigned long)SHIFT_REG_SERIAL_HZ,
           (unsigned long)(LED_FRAME_BYTES * 8UL * 1000000UL / SHIFT_REG_SERIAL_HZ));
    printf("\r\n  OE PWM %lu Hz, ARR %lu",
           (unsigned long)BRIGHTNESS_PWM_HZ,
           (unsigned long)((uint64_t)CPU_HZ / (uint64_t)BRIGHTNESS_PWM_HZ - (uint64_t)1));

    for (uint16_t i = 0; i < device_count; i++)
    {
        const led_device_t *d = &devices[i];
        printf("\r\n   - LED_DEVICE %d : OFFSET=%d, WIDTH=%d SCALE=%s, DISP_MODE=%s",
               (unsigned)i, (unsigned)d->offset, (unsigned)d->width,
               d->scale ? "center" : "left",
               d->disp_mode ? "point" : "bar");
    }
    printf("\n");
}
#else
void led_print_config (void) { }
#endif


/*=============================== ANIMATIONS ================================*/
// Frame refreshers: call repeatedly, and one call is one step. These do not change the actual stored value

static anim_t   anim_active = ANIM_NONE;
static uint32_t anim_bits   = LED_ANIM_LOAD_PATTERN;
static uint16_t anim_bphase = 0;

/**
 ----------------------------------------------------------------------------------
  @brief anim_claim : Run this before starting any animation
 ----------------------------------------------------------------------------------
*/
static void anim_claim (anim_t a)
{
    if (anim_active != a) anim_stop();
    anim_active = a;
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_stop : Run this after animation to return to regular LED operation
 ----------------------------------------------------------------------------------
*/
void anim_stop (void)
{
    anim_active = ANIM_NONE;
    anim_bits   = LED_ANIM_LOAD_PATTERN;
    anim_bphase = 0;

    for (uint16_t i = 0; i < device_count; i++)
    {
        led_release(&devices[i]); // redraws from the stored value
    }

    oe_duty(bright);
    frame_dirty = true;
    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_loading : Renders frames for the loading animation
 ----------------------------------------------------------------------------------
*/
void anim_loading (void)
{
    anim_claim(ANIM_LOAD);
    anim_bits = (anim_bits >> 1) | (anim_bits << 31); // rotate the pattern one step

    for (uint16_t i = 0; i < device_count; i++)
    {
        led_device_t *d = &devices[i];
        led_raw(d, anim_bits);
    }

    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_sweep : Renders frames for the sweep animation
 ----------------------------------------------------------------------------------
*/
void anim_sweep (led_device_t *device, uint16_t phase)
{
    anim_claim(ANIM_SWEEP);

    const uint16_t w      = device->width;
    const uint16_t period = (uint16_t)(2U * w);

    if (phase >= period) phase = 0u;
    const uint16_t lit = (phase <= w) ? phase : (uint16_t)(period - phase);

    uint32_t bits = 0u;
    if ((knob_scale_t)device->scale == SCALE_LEFT)
    {
        for (uint16_t i = 0; i < lit && i < 32u; i++) bits |= (1UL << i);
    }
    else // center-scale rings sweep outwards from the middle, matching their display mode
    {
        const uint16_t c    = (device->center < w) ? device->center : (uint16_t)(w - 1u);
        const uint16_t half = (uint16_t)(lit / 2u);
        const uint16_t lo   = (c > half) ? (uint16_t)(c - half) : 0u;
        const uint16_t hi   = ((c + half) < w) ? (uint16_t)(c + half) : (uint16_t)(w - 1u);
        for (uint16_t i = lo; i <= hi && i < 32u; i++) bits |= (1UL << i);
    }

    led_raw(device, bits);
    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_breathe : Renders frames for the breathing animation
 ----------------------------------------------------------------------------------
*/
void anim_breathe (uint8_t mode)
{
    anim_claim(mode ? ANIM_BREATHE_ALL : ANIM_BREATHE_KEEP);

    if (mode) // every LED on, values unchanged underneath
    {
        for (uint16_t i = 0; i < device_count; i++) led_raw(&devices[i], 0xFFFFFFFFUL);
    }

    const uint16_t p = (uint16_t)(anim_bphase % (LED_ANIM_BREATHE_STEPS * 2U));
    const uint16_t x = (p < LED_ANIM_BREATHE_STEPS) ? p : (uint16_t)(LED_ANIM_BREATHE_STEPS * 2U - p);
    const uint32_t g = (uint32_t)x * x; // squared ramp reads as more even fading to the eye lol
    const uint8_t  lvl = (uint8_t)(LED_ANIM_BREATHE_MIN +
                         (uint32_t)(LED_ANIM_BREATHE_MAX - LED_ANIM_BREATHE_MIN) * g
                         / (LED_ANIM_BREATHE_STEPS * LED_ANIM_BREATHE_STEPS));

    oe_duty(lvl); // not led_brightness, this must not overwrite the real setting
    anim_bphase++;
    led_update();
}