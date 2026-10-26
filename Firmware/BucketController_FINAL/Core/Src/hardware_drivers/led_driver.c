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

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/74hc595.h"
#include "CONFIG.h"
#include "main.h"

#if DEVELOPER_MODE
#include <stdio.h>
#define LED_LOG(...) printf(__VA_ARGS__)
#else
#define LED_LOG(...) ((void)0)
#endif


/*=============================== LED DEVICE CHAIN DEFN ================================*/
// THIS IS WHERE THE PHYSICAL GEOMETRY / HARDWARE LAYOUT IS DEFINED IN SOFTWARE
led_device_init_t init_devices[] = {

    {.width = 32, .scale = SCALE_LEFT}, // ch 0 input knob
    
    // external shiftreg 1
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 input line btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 input mic btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 input hi-z btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 input switch btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 phase
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 phantom
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 HPF
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 eq

    //external shiftreg 2
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 input line btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 input mic btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 input hi-z btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 input switch btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 phase
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 phantom
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 HPF
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 eq

    {.width = 32, .scale = SCALE_LEFT}, // ch 1 input knob
    {.width = 32, .scale = SCALE_LEFT}, // ch 2 input knob

    // external shiftreg 3
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 input line btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 input mic btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 input hi-z btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 input switch btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 phase
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 phantom
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 HPF
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 eq

    //external shiftreg 4
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 input line btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 input mic btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 input hi-z btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 input switch btn
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 phase
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 phantom
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 HPF
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 eq

    {.width = 32, .scale = SCALE_LEFT}, // ch 3 input knob

    // FX SECTION KNOBS, ROW 1A
    {.width = 32, .scale = SCALE_CENTER}, // ch 3
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 0

    // FX SECTION KNOBS, ROW 1B
    {.width = 32, .scale = SCALE_CENTER}, // ch 0
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 3

    // FX SECTION KNOBS, ROW 2A
    {.width = 32, .scale = SCALE_CENTER}, // ch 3
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 0

    // FX SECTION KNOBS, ROW 2B
    {.width = 32, .scale = SCALE_CENTER}, // ch 0
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 3

    // FX SECTION KNOBS, ROW 3A
    {.width = 32, .scale = SCALE_CENTER}, // ch 3
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 0

    // FX SECTION KNOBS, ROW 3B
    {.width = 32, .scale = SCALE_CENTER}, // ch 0
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 3

    {.width = 32, .scale = SCALE_CENTER}, // ch 3 FX 4A
    
    // external shiftreg 5
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 comp
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 sends
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 SOLO
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 MUTE
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 SEL
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 insert
    {.width = 1, .scale = SCALE_CENTER}, // ch 2 insert pre
    {.width = 1, .scale = SCALE_CENTER}, // EMPTY N/C

    //external shiftreg 6
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 comp
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 sends
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 SOLO
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 MUTE
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 SEL
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 insert
    {.width = 1, .scale = SCALE_CENTER}, // ch 3 insert pre
    {.width = 1, .scale = SCALE_CENTER}, // EMPTY N/C

    {.width = 32, .scale = SCALE_CENTER}, // ch 2 FX 4A
    {.width = 32, .scale = SCALE_CENTER}, // ch 1 FX 4A

    // external shiftreg 7
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 comp
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 sends
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 SOLO
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 MUTE
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 SEL
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 insert
    {.width = 1, .scale = SCALE_CENTER}, // ch 0 insert pre
    {.width = 1, .scale = SCALE_CENTER}, // EMPTY N/C

    //external shiftreg 8
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 comp
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 sends
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 SOLO
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 MUTE
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 SEL
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 insert
    {.width = 1, .scale = SCALE_CENTER}, // ch 1 insert pre
    {.width = 1, .scale = SCALE_CENTER}, // EMPTY N/C

    {.width = 32, .scale = SCALE_CENTER}, // ch 0 FX 4A

    // FX SECTION KNOBS, ROW 4B
    {.width = 32, .scale = SCALE_CENTER}, // ch 0
    {.width = 32, .scale = SCALE_CENTER}, // ch 1
    {.width = 32, .scale = SCALE_CENTER}, // ch 2
    {.width = 32, .scale = SCALE_CENTER}, // ch 3

    // PANS
    {.width = 32, .scale = SCALE_CENTER}, // ch 0 pan
    {.width = 32, .scale = SCALE_CENTER}, // ch 1 pan
    {.width = 32, .scale = SCALE_CENTER}, // ch 2 pan
    {.width = 32, .scale = SCALE_CENTER}, // ch 3 pan

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

extern SPI_HandleTypeDef LED_SPI_HANDLE;  // from main.c
extern TIM_HandleTypeDef LED_OE_TIM_HANDLE; // from main.c

static uint8_t       txbuf[LED_FRAME_BYTES];
// static volatile bool tx_busy = false;

static anim_t   anim_active = ANIM_NONE;
static uint32_t anim_bits   = LED_ANIM_LOAD_PATTERN;
static uint16_t anim_bphase = 0;

static volatile bool loading    = false;
static uint8_t       load_div   = 0;

#ifndef LED_ANIM_LOAD_DIV
#define LED_ANIM_LOAD_DIV 7U // frame divider to slow the load animation
#endif


/*=============================== USE LED SPI DMA ================================*/

static inline void oe_hold_dark (bool dark)
{
    TIM_TypeDef *t = LED_OE_TIM_HANDLE.Instance;
    uint32_t ccmr = t->CCMR1 & ~TIM_CCMR1_OC2M;
    ccmr |= dark ? (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_0)   // forced active: OE high, LEDs dark
                 : (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1);  // back to PWM mode 1
    t->CCMR1 = ccmr;
}

void HAL_SPI_TxCpltCallback (SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == LED_SPI_INSTANCE) // prevents interference if using DMA for another SPI at the same time
    {
        latch_out(true);
        oe_hold_dark(false); // LEDs back on, showing the new frame
        // tx_busy = false;
    }
}


/*=============================== BIT / SPAN PRIMITIVES ================================*/

// set or clear one bit of a buffer, counting from the MSB of byte 0
static inline void buf_bit (uint8_t *buf, uint16_t b, bool on)
{
    const uint16_t byte = (uint16_t)(LED_FRAME_BYTES - 1u - (b >> 3));
    const uint8_t  mask = (uint8_t)(1u << (b & 7u));
    if (on) buf[byte] |=  mask;
    else    buf[byte] &= (uint8_t)~mask;
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
        return; // writing past devices[] would corrupt memory
    }

    if ((uint32_t)used_bits + width > (uint32_t)LED_CHAIN_BITS)
    {
        LED_LOG("LED ERR: chain overflow at device %u (%u + %u > %u bits)\n",
                (unsigned)device_count, (unsigned)used_bits,
                (unsigned)width, (unsigned)LED_CHAIN_BITS);
        return; // its bits would land outside the frame buffer
    }

    led_device_t *device = &devices[device_count++];

    device->offset    = used_bits;
    device->width     = width;
    device->center    = 16;
    device->value     = 0U; // initial value to display is 0
    device->disp_mode = 0;  // default display mode is bar right now
    device->scale     = scale;
    device->raw       = false;
    device->dirty     = true;
    frame_dirty       = true; // so the next led_update() draws it

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
    frame_dirty   = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_signed : Set signed LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_signed (led_device_t *device, int16_t value)
{
    led_set(device, (uint16_t)value);
}


/**
 ----------------------------------------------------------------------------------
  @brief led_set_bool : Set a button LED display device's value! important!!
 ----------------------------------------------------------------------------------
*/
void led_set_bool (led_device_t *device, bool on)
{
    led_set(device, on ? UINT16_MAX : 0U);
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
    device->raw   = true;
    device->dirty = false; // bits are already in the frame, nothing to render
    frame_dirty = true;
}


/**
 ----------------------------------------------------------------------------------
  @brief led_release : hand the device back to its renderer and redraw from value
 ----------------------------------------------------------------------------------
*/
void led_release (led_device_t *device)
{
    if (device == NULL) return;
    device->raw   = false;
    device->dirty = true;
    frame_dirty   = true;
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
        if (!d->dirty || d->raw) continue; // skips devices whos values have not changed and raw writes

        if (d->width == 1) // device is a button led / boolean led
        {
            led_render_bool(d);
        } else {
            led_render(d);
        }
        d->dirty = false; // that device is now clean yay
    }
}


/*=============================== REV0 HARDWARE WORKAROUNDS LMAO ================================*/
// Bits never sent lit on rev0 hardware: lit, they trigger extra clock pulses from the chain's
// own switching on a marginal clock edge. Applied to the transmit copy only, so the frame and
// every device's state stay correct. Delete once the clock line is fixed in hardware.
static const struct { uint8_t device; uint8_t led; } TX_NEVER_LIT[] = {
    {78, 31}, // ch1 4A rightmost LED: when lit, LEDs 16-19 of the same ring go dark
};

static void tx_apply_workarounds (void)
{
    for (uint8_t i = 0; i < sizeof(TX_NEVER_LIT) / sizeof(TX_NEVER_LIT[0]); i++)
    {
        if (TX_NEVER_LIT[i].device >= device_count) continue;
        const led_device_t *d = &devices[TX_NEVER_LIT[i].device];
        if (TX_NEVER_LIT[i].led >= d->width) continue;
        buf_bit(txbuf, (uint16_t)(d->offset + TX_NEVER_LIT[i].led), false);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief frame_out : shift the whole frame out (serial data), then latch. USING DMA for fast (the LED SPI's DMA).
 ----------------------------------------------------------------------------------
*/
static void frame_out (void)
{
    // if (tx_busy) { frame_dirty = true; return; } // retry next tick

    memcpy(txbuf, frame, LED_FRAME_BYTES);
    tx_apply_workarounds();
    oe_hold_dark(true); // nothing switches while bits shift

    if (HAL_SPI_Transmit_DMA(&LED_SPI_HANDLE, txbuf, LED_FRAME_BYTES) != HAL_OK)
    {
        oe_hold_dark(false);
        frame_dirty = true;
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief led_update : important!! render values, push the data out, and latch it
 ----------------------------------------------------------------------------------
*/
void led_update (void)
{
    if (loading && ++load_div >= LED_ANIM_LOAD_DIV)
    {
        load_div = 0;
        anim_loading();
    }

    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    bool work = frame_dirty;
    frame_dirty = false;
    __set_PRIMASK(prim);

    if (!work) return;
    render_dirty();

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
    // while (tx_busy) {}
}


led_device_t *led_device_at (uint8_t idx)
{
    if (idx >= device_count) return NULL; // bounds checks now lol
    return &devices[idx];
}


/*=============================== BRIGHTNESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief oe_duty : brightness 0-255 -> PWM duty cycle. The compare register holds the number of
  timer ticks per PWM cycle to keep the OE pin high, and OE high blanks the display,
  so a higher compare value is dimmer.
 ----------------------------------------------------------------------------------
*/
static void oe_duty (uint8_t brightness)
{
    TIM_HandleTypeDef *oe = &LED_OE_TIM_HANDLE;
    __HAL_TIM_SET_COMPARE(oe, LED_OE_TIM_CHAN,
                          ((uint32_t)(255U - brightness) * (__HAL_TIM_GET_AUTORELOAD(oe) + 1U)) / 255U);
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

uint8_t  led_get_brightness (void) { return bright; }


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

    /* Brightness is a PWM on the OE pin. CubeMX owns the timer mode, preload and pin alternate
       function; here we only set the period from SystemCoreClock and start it. */
    TIM_HandleTypeDef *oe = &LED_OE_TIM_HANDLE;

    // period: e.g. 170MHz / 24kHz = 7,083 timer ticks per PWM cycle (assumes timer clock = CPU clock)
    __HAL_TIM_SET_AUTORELOAD(oe, ((uint64_t)SystemCoreClock / (uint64_t)BRIGHTNESS_PWM_HZ) - 1U);

    // start blanked (PWM always high = 0 brightness) until led_brightness runs
    __HAL_TIM_SET_COMPARE(oe, LED_OE_TIM_CHAN, __HAL_TIM_GET_AUTORELOAD(oe) + 1U);

    // force an update event so the new period and compare take effect, then clear the flag it sets
    HAL_TIM_GenerateEvent(oe, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_CLEAR_FLAG(oe, TIM_FLAG_UPDATE);

    HAL_TIM_PWM_Start(oe, LED_OE_TIM_CHAN);

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
    printf("\r\n  OE PWM %lu Hz, ARR %lu",
           (unsigned long)BRIGHTNESS_PWM_HZ,
           (unsigned long)((uint64_t)SystemCoreClock / (uint64_t)BRIGHTNESS_PWM_HZ - (uint64_t)1));

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
}


/**
 ----------------------------------------------------------------------------------
  @brief anim_loading : Renders frames for the loading animation
 ----------------------------------------------------------------------------------
*/
void anim_loading (void)
{
    anim_claim(ANIM_LOAD);
    anim_bits = (anim_bits << 1) | (anim_bits >> 31); // rotate the pattern one step

    for (uint16_t i = 0; i < device_count; i++)
    {
        led_device_t *d = &devices[i];
        led_raw(d, anim_bits);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief led_loading : RUNS LOADING ANIMATION from global loading flag
 ----------------------------------------------------------------------------------
*/
void led_loading (bool on)
{
    if (on == loading) return;
    loading  = on;
    load_div = 0;

    if (!on) anim_stop(); // releases all devices, rtn to normal op
    frame_dirty = true;
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
}


/**
 ----------------------------------------------------------------------------------
  @brief led_test_all_on : TEST ONLY. Lights every output of every 595 in the chain
  and keeps them lit, for spotting dead LEDs. Includes outputs no device is registered
  for, so it doesn't depend on the layout table being right. Reset to go back to normal.
 ----------------------------------------------------------------------------------
*/
void led_test_all_on (uint8_t brightness)
{
    led_loading(false); // the loading animation redraws every device, even held ones
    anim_stop();        // and any other animation

    for (uint16_t i = 0; i < device_count; i++)
    {
        devices[i].raw = true; // held: the renderer skips it, so knob changes can't redraw it
    }
    memset(frame, 0xFF, LED_FRAME_BYTES); // every output of every 595 on
    led_brightness(brightness);
    frame_dirty = true; // the LED task's next led_update() shifts it out
}