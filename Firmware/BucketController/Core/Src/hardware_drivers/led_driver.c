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


/*=============================== STATE ================================*/
// ALL data stored by driver is here

static uint8_t      frame[LED_FRAME_BYTES];   // one bit per LED, in cascade order, ready to shift out
static led_device_t devices[LED_MAX_DEVICES]; // every display element on the chain, in cascade order
static uint16_t     device_count = 0;         // how many of the above are in use
static uint16_t     used_bits    = 0;         // sum of their widths, so the next led_add knows its offset
static bool         chain_closed = false;     // led_chain_finalize has run, no more devices accepted

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
  @brief led_span_set : set LED i of a device, 0..width-1 in LED order.
  This is where wiring quirks are absorbed: reverse flips the order so a ring
  soldered backwards still counts from its own LED 0, and invert handles hardware
  that sinks current, where a lit LED means a low output.
 ----------------------------------------------------------------------------------
*/
void led_span_set (led_span_t sp, uint16_t i, bool on)
{
    if (i >= sp.width) return;
    const uint16_t b = (uint16_t)(sp.offset + (sp.reverse ? (sp.width - 1u - i) : i));
    buf_bit(frame, b, sp.invert ? !on : on);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_span_clear : turn off every LED belonging to a device
 ----------------------------------------------------------------------------------
*/
void led_span_clear (led_span_t sp)
{
    for (uint16_t i = 0; i < sp.width; i++) led_span_set(sp, i, false);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_span_fill : turn on a run of LEDs, lo..hi inclusive
 ----------------------------------------------------------------------------------
*/
void led_span_fill (led_span_t sp, uint16_t lo, uint16_t hi)
{
    if (lo >= sp.width) return;
    if (hi >= sp.width) hi = (uint16_t)(sp.width - 1u);
    for (uint16_t i = lo; i <= hi; i++) led_span_set(sp, i, true);
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
  @brief led_render_meter : default renderer, for anything with more than one
  LED. turns the core control value into a set of bits to display the value on LEDs.
  Puts these bits in the frame buffer to appear at the LEDs corresponding to that control
 ----------------------------------------------------------------------------------
*/
void led_render_meter (const led_device_t *dev)
{
    const led_span_t sp = dev->span;

    led_span_clear(sp);
    if (sp.width == 0u) return;

    if ((knob_scale_t)dev->scale == SCALE_LEFT)
    {
        // value is unsigned: the bar grows from left LED
        const uint16_t n = frac(dev->value, UINT16_MAX, sp.width);
        if (n == 0u) return;
        if ((knob_disp_t)dev->disp == DISP_POINT) led_span_set(sp, (uint16_t)(n - 1u), true);
        else                                      led_span_fill(sp, 0u, (uint16_t)(n - 1u));
    }
    else
    {
        // value is signed: the bar grows outwards from the center LED, which is unity
        const int32_t  v = (int32_t)(int16_t)dev->value;
        const uint16_t c = (dev->center < sp.width) ? dev->center : (uint16_t)(sp.width - 1u);

        if (v >= 0)
        {
            const uint16_t n = frac((uint32_t)v, 32767u, (uint16_t)(sp.width - 1u - c));
            if ((knob_disp_t)dev->disp == DISP_POINT) led_span_set(sp, (uint16_t)(c + n), true);
            else                                      led_span_fill(sp, c, (uint16_t)(c + n));
        }
        else
        {
            const uint16_t n = frac((uint32_t)(-v), 32768u, c);
            if ((knob_disp_t)dev->disp == DISP_POINT) led_span_set(sp, (uint16_t)(c - n), true);
            else                                      led_span_fill(sp, (uint16_t)(c - n), c);
        }
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief led_render_bool : renders buttons (width 1)
 ----------------------------------------------------------------------------------
*/
void led_render_bool (const led_device_t *dev)
{
    if (dev->value) led_span_fill(dev->span, 0u, (uint16_t)(dev->span.width - 1u));
    else            led_span_clear(dev->span);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_render_blank : sets all off, for unpopulated outputs and chain padding
 ----------------------------------------------------------------------------------
*/
void led_render_blank (const led_device_t *dev)
{
    led_span_clear(dev->span);
}


/*=============================== CHAIN BUILD ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_add : append one device to the chain. Call order is cascade order, so
  a device's index in the list is where it sits on the board, and its bit offset is
  just the running total of everything registered before it.
 ----------------------------------------------------------------------------------
*/
led_device_t *led_add (const led_device_cfg_t *cfg)
{
    if (cfg == NULL || cfg->width == 0u) return NULL;

    if (chain_closed)
    {
        LED_LOG("LED ERR: led_add after finalize\n");
        return NULL;
    }
    if (device_count >= LED_MAX_DEVICES)
    {
        LED_LOG("LED ERR: device table full (%u), raise LED_MAX_DEVICES\n", (unsigned)LED_MAX_DEVICES);
        return NULL;
    }
    if ((uint32_t)used_bits + cfg->width > (uint32_t)LED_CHAIN_BITS)
    {
        LED_LOG("LED ERR: chain overflow at device %u (%u + %u > %u bits)\n",
                (unsigned)device_count, (unsigned)used_bits,
                (unsigned)cfg->width, (unsigned)LED_CHAIN_BITS);
        return NULL;
    }

    led_device_t *dev = &devices[device_count++];

    dev->span.offset  = used_bits;
    dev->span.width   = cfg->width;
    dev->span.reverse = (cfg->flags & LED_F_REVERSE) ? 1u : 0u;
    dev->span.invert  = (cfg->flags & LED_F_INVERT)  ? 1u : 0u;

    dev->center  = (cfg->centre != 0u) ? cfg->centre : (uint16_t)((cfg->width - 1u) / 2u);
    dev->value   = 0u;
    dev->render  = (cfg->render != NULL) ? cfg->render : led_render_meter;
    dev->role    = cfg->role;
    dev->group   = cfg->group;
    dev->ordinal = cfg->ordinal;
    dev->disp    = (cfg->flags & LED_F_POINT)  ? 1u : 0u;
    dev->scale   = (cfg->flags & LED_F_CENTER) ? 1u : 0u;
    dev->dirty   = 1u;
    dev->held    = 0u;
    dev->anim_phase = 0u;

    used_bits = (uint16_t)(used_bits + cfg->width);
    return dev;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_chain_finalize : fill everything to full bytes (empty pins on 74HCT595s)
  All nonpopulated bits are set to 0 before being pushed out (instead of floating / garbage)
 ----------------------------------------------------------------------------------
*/
bool led_chain_finalize (void)
{
    const uint16_t pad = (uint16_t)((8u - (used_bits & 7u)) & 7u);

    if (pad != 0u)
    {
        const led_device_cfg_t filler = {
            .width  = pad,
            .render = led_render_blank,
            .role   = LED_ROLE_NONE,
        };
        if (led_add(&filler) == NULL) return false;
    }

    chain_closed = true;

    if (used_bits > LED_CHAIN_BITS)
    {
        LED_LOG("LED ERR: layout needs %u bits, LED_CHAIN_BITS is %u\n",
                (unsigned)used_bits, (unsigned)LED_CHAIN_BITS);
        return false;
    }
    if (used_bits < LED_CHAIN_BITS)
    {
        LED_LOG("LED WARN: layout uses %u of %u chain bits, %u registers idle\n",
                (unsigned)used_bits, (unsigned)LED_CHAIN_BITS,
                (unsigned)((LED_CHAIN_BITS - used_bits) / 8u));
    }
    return true;
}


/*=============================== VALUES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief mark : frame in frame buffer now dirty
 ----------------------------------------------------------------------------------
*/
static inline void mark (led_device_t *dev)
{
    dev->dirty  = 1u;
    frame_dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set : Set LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set (led_device_t *dev, uint16_t value)
{
    if (dev == NULL || dev->value == value) return;
    dev->value = value;
    mark(dev);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_signed : Set signed LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_signed (led_device_t *dev, int16_t value) { led_set(dev, (uint16_t)value); }


/**
 ----------------------------------------------------------------------------------
  @brief led_set_bool : Set a button LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_bool (led_device_t *dev, bool on) { led_set(dev, on ? UINT16_MAX : 0u); }


/**
 ----------------------------------------------------------------------------------
  @brief led_get : get the displayed value from any LED device
 ----------------------------------------------------------------------------------
*/
uint16_t led_get (led_device_t *dev) { return (dev != NULL) ? dev->value : 0u; }


/*=============================== MODES ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_set_disp : Set display mode. 0 = bar fill, 1 = point
 ----------------------------------------------------------------------------------
*/
void led_set_disp (led_device_t *dev, knob_disp_t mode)
{
    if (dev == NULL || dev->disp == (uint8_t)(mode & 1u)) return;
    dev->disp = (uint8_t)(mode & 1u);
    mark(dev);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_scale : Set scale mode. 0 = from left, 1 = signed / from center
 ----------------------------------------------------------------------------------
*/
void led_set_scale (led_device_t *dev, knob_scale_t mode)
{
    if (dev == NULL || dev->scale == (uint8_t)(mode & 1u)) return;
    dev->scale = (uint8_t)(mode & 1u);
    mark(dev);
}


/*=============================== RAW ACCESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief led_raw : draw an arbitrary pattern and suspend the renderer for this
  device, so the stored control value survives untouched underneath. Animations use
  this; led_release hands the device back and redraws it from its value.
 ----------------------------------------------------------------------------------
*/
void led_raw (led_device_t *dev, uint32_t bits)
{
    if (dev == NULL) return;

    const led_span_t sp = dev->span;
    for (uint16_t i = 0; i < sp.width; i++)
    {
        led_span_set(sp, i, (i < 32u) ? (((bits >> i) & 1u) != 0u) : false);
    }
    dev->held   = 1u;
    dev->dirty  = 0u;
    frame_dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_release : release an led_device from raw renderer / return to regular renderer
 ----------------------------------------------------------------------------------
*/
void led_release (led_device_t *dev)
{
    if (dev == NULL || !dev->held) return;
    dev->held = 0u;
    mark(dev);
}


/*=============================== OUTPUT ================================*/

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
}


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
        if (!d->dirty || d->held) continue;
        d->render(d);
        d->dirty = 0u;
    }
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

    for (uint16_t i = 0; i < device_count; i++)
    {
        devices[i].value = 0u; // 0 is empty on SCALE_LEFT and centred on SCALE_CENTER
        devices[i].held  = 0u;
        devices[i].dirty = 1u;
    }

    render_dirty(); // active-low devices need their bits driven high to be dark
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
    chain_closed = false;

    led_shiftreg_init();

    for (uint16_t i = 0; i < LED_FRAME_BYTES; i++) frame[i] = 0x00;
    frame_out(); // everything off before the PWM comes up

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
}


/**
 ----------------------------------------------------------------------------------
  @brief led_print_config : dev mode stuff
 ----------------------------------------------------------------------------------
*/
#if DEVELOPER_MODE
void led_print_config (void)
{
    static const char *role_name[] = {"-", "ring", "btn", "bar", "misc"};

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
        printf("\r\n  [%3u] bit %4u +%-3u %-4s ch%u #%-2u %s %s%s%s",
               (unsigned)i, (unsigned)d->span.offset, (unsigned)d->span.width,
               role_name[(d->role < 5u) ? d->role : 4u],
               (unsigned)d->group, (unsigned)d->ordinal,
               d->scale ? "centre" : "left  ",
               d->disp ? "point" : "bar",
               d->span.reverse ? " rev" : "",
               d->span.invert ? " inv" : "");
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
        devices[i].anim_phase = 0u;
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
void anim_loading (int8_t ordinal)
{
    anim_claim(ANIM_LOAD);
    anim_bits = (anim_bits >> 1) | (anim_bits << 31); // rotate the pattern one step

    for (uint16_t i = 0; i < device_count; i++)
    {
        led_device_t *d = &devices[i];
        if (d->role != (uint8_t)LED_ROLE_RING) continue;
        led_raw(d, (ordinal < 0 || d->ordinal == (uint8_t)ordinal) ? anim_bits : 0U);
    }

    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_sweep : Renders frames for the sweep animation
 ----------------------------------------------------------------------------------
*/
void anim_sweep (led_device_t *dev)
{
    if (dev == NULL || dev->span.width == 0u) return;
    anim_claim(ANIM_SWEEP);

    const uint16_t w      = dev->span.width;
    const uint16_t period = (uint16_t)(2u * w);
    uint16_t       ph     = dev->anim_phase;

    if (ph >= period) ph = 0u;
    const uint16_t lit = (ph <= w) ? ph : (uint16_t)(period - ph);

    uint32_t bits = 0u;
    if ((knob_scale_t)dev->scale == SCALE_LEFT)
    {
        for (uint16_t i = 0; i < lit && i < 32u; i++) bits |= (1UL << i);
    }
    else // center-scale rings sweep outwards from the middle, matching their display mode
    {
        const uint16_t c    = (dev->center < w) ? dev->center : (uint16_t)(w - 1u);
        const uint16_t half = (uint16_t)(lit / 2u);
        const uint16_t lo   = (c > half) ? (uint16_t)(c - half) : 0u;
        const uint16_t hi   = ((c + half) < w) ? (uint16_t)(c + half) : (uint16_t)(w - 1u);
        for (uint16_t i = lo; i <= hi && i < 32u; i++) bits |= (1UL << i);
    }

    dev->anim_phase = (uint8_t)(ph + 1u);
    led_raw(dev, bits);
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