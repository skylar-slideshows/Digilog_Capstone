/**
  **********************************************************************************
  * LED DISPLAYS DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file led_driver.c
  * @brief Skylar's driver for the LED displays on the console, including knobs and buttons.
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

#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/74hc595.h"
#include "stm32g4xx.h"
#include "CONFIG.h"
#include <stdio.h>
#include <stdbool.h>


/*=============================== DEFINE CALCS ================================*/
// dont touch ; change in config.h

// Sizes
#define KNOB_BYTES  (LEDS_PER_KNOB / 8)
#define BTN_BYTES   (BUTTONS_PER_CHAN / 8)
#define CHAN_BYTES  (KNOBS_PER_CHAN * KNOB_BYTES + BTN_BYTES)
#define FRAME_BYTES (CHANNELS * CHAN_BYTES)
#define TOTAL_LEDS  (CHANNELS * (LEDS_PER_KNOB * KNOBS_PER_CHAN + BUTTONS_PER_CHAN))
#define NUM_KNOBS   (CHANNELS * KNOBS_PER_CHAN)

// Knob center
#define CENTER_MIN   (-(int8_t)CENTER_LED)
#define CENTER_MAX   ((int8_t)(LEDS_PER_KNOB - 1 - CENTER_LED))

// Manners
#define PLEASE
#define PRETTY_PLEASE


/*=============================== STATE ================================*/
/* All data stored by the driver is here */

static uint8_t  frame[FRAME_BYTES];
static knob_t   knobs[NUM_KNOBS];
static uint8_t  bright = 255;
static volatile bool dirty = true;


/*=============================== FRAME BUFFER ================================*/
/* Knob states / values -> sets of led bits ready to send out */

#define KNOB_OFS(channel_idx, knob_idx) ((channel_idx) * CHAN_BYTES + (knob_idx) * KNOB_BYTES)
#define BTN_OFS(channel_idx)((channel_idx) * CHAN_BYTES + KNOBS_PER_CHAN * KNOB_BYTES)


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC frame_out : Sends an entire frame of LED bits out, end sent first
 ----------------------------------------------------------------------------------
*/
void frame_out(void)
{
    uint16_t n = FRAME_BYTES;
    while (n--)
    {
        uint8_t b = frame[n];
        for (int8_t i = 0; i < 8; i++) shift_bit((uint32_t)((b >> i) & 1U), 1);
    }
    latch_out(1);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL render_val : Channel 0-3 (uint8), Knob 0-9 (uint8), value (0-32 or -15 to 16),
  knob_scale_t, knob_disp_t -> 32 bits in the frame
  Directly render: turns a knob's value into the set of bits that show that value
 ----------------------------------------------------------------------------------
*/
static void render_val(uint8_t channel_idx, uint8_t knob_idx, int8_t val, uint8_t scale, uint8_t disp)
{
    uint8_t *p = &frame[KNOB_OFS(channel_idx, knob_idx)]; // pointer to the 32 bits, offset in the frame buffer to appear at this knob
    uint8_t lo, hi, i; // indicates where lit strip (or single) leds start and stop and an index for later sorry for short names

    p[0] = p[1] = p[2] = p[3] = 0x00; // replace whole ring so clear em

    if (scale == SCALE_LEFT)
    {
        if (val <= 0) return;
        hi = (uint8_t)(val - 1);
        lo = (disp == DISP_POINT) ? hi : 0;
    } else {
        uint8_t t = (uint8_t)((int8_t)CENTER_LED + val);
        if (disp == DISP_POINT) { lo = hi = t; }
        else if (t < CENTER_LED) { lo = t; hi = CENTER_LED; } // point is in left half, knob turned left from center, light t to center
        else { lo = CENTER_LED; hi = t; } // right half or center
    }
    for (i = lo; i <= hi; i++) p[i >> 3] |= (uint8_t)(0x80U >> (i & 7)); // turn the lo and hi endpoints + span into bits
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL knob_render : Channel 0-3 (uint8), Knob 0-9 (uint8) -> Void
  Unpack the params inside a knob at a coord and use render_val.
 ----------------------------------------------------------------------------------
*/
static void knob_render(uint8_t channel_idx, uint8_t knob_idx)
{
    knob_t *knob_p = &knobs[channel_idx * KNOBS_PER_CHAN + knob_idx];
    render_val(channel_idx, knob_idx, knob_p->val, knob_p->scale, knob_p->disp);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL ck_knob : Channel 0-3 (uint8), Knob 0-9 (uint8),
                    Channel 0-3 (ptr uint8), Knob 0-9 (ptr uint8) -> Bool.
  Range checks external knob/channel coords and copies them out
 ----------------------------------------------------------------------------------
*/
static bool ck_knob(uint8_t channel, uint8_t knob, uint8_t *channel_idx, uint8_t *knob_idx)
{
    if (channel >= CHANNELS || knob >= KNOBS_PER_CHAN) {
        printf("LED ERR: bad knob coord ch%u k%u (max ch%u k%u)\n",
            channel, knob, CHANNELS - 1, KNOBS_PER_CHAN - 1);
        return false;
    }
    *channel_idx = channel;
    *knob_idx = knob;
    return true;
}


/*=============================== KNOBS HIGHER LEVEL FN'S ================================*/
/* Public functions to set knob states */

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_led : Channel 0-3 (uint8), Knob 0-9 (uint8), Value 0 to 32 or -15 to 16
  Change a knob's display value, specify which knob by coords (chnl and knob number)
 ----------------------------------------------------------------------------------
*/
void knob_led(uint8_t channel, uint8_t knob, int8_t value)
{
    uint8_t channel_idx, knob_idx;
    knob_t *knob_p;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return; // given coords bad
    knob_p = &knobs[channel_idx * KNOBS_PER_CHAN + knob_idx];

    int8_t lo = (knob_p->scale == SCALE_LEFT) ? 0 : CENTER_MIN; // lower value limit is 0 for left mode and -15 for center mode
    int8_t hi = (knob_p->scale == SCALE_LEFT) ? LEDS_PER_KNOB : CENTER_MAX;

    if (value < lo || value > hi)
    {
        /*if(DEVELOPER_MODE) printf("LED ERR: ch%u k%u value %d outside [%d,%d], clamped\n",
            channel, knob, value, lo, hi);*/
        value = (value < lo) ? lo : hi;
    }

    knob_p->val = value;
    knob_render(channel_idx, knob_idx);
    dirty = true;
    /*if(DEVELOPER_MODE) printf("LED: ch%u k%u = %d (%s/%s)\n", channel, knob, value,
        knob_p->scale ? "CENTER" : "LEFT", knob_p->disp ? "POINT" : "BAR");*/
}

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_step : Channel 0-3 (uint8), Knob 0-9 (uint8), int Delta -> Void
  Incerment/decrement a knob's display value.
 ----------------------------------------------------------------------------------
*/
void knob_step(uint8_t channel, uint8_t knob, int8_t delta)
{
    uint8_t channel_idx, knob_idx;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return;
    if(knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].val + delta > 32)
    {
        knob_led(channel, knob, (int8_t)(knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].val + delta));
    }
    return;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_disp : Set display mode of knob
 ----------------------------------------------------------------------------------
*/
void knob_disp(uint8_t channel, uint8_t knob, knob_disp_t mode)
{
    uint8_t channel_idx, knob_idx;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return;
    knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].disp = (uint8_t)(mode & 1u);
    knob_render(channel_idx, knob_idx);
    dirty = true;
    //if(DEVELOPER_MODE) printf("LED: ch%u k%u display = %s\n", channel, knob, mode ? "POINT" : "BAR");
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_scale : set scale mode of knob (left to right or center/pan)
 ----------------------------------------------------------------------------------
*/
void knob_scale(uint8_t channel, uint8_t knob, knob_scale_t mode)
{
    uint8_t channel_idx, knob_idx;
    knob_t *knob_p;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return;
    knob_p = &knobs[channel_idx * KNOBS_PER_CHAN + knob_idx];
    if ((knob_scale_t)knob_p->scale != mode) {
        int16_t v = (mode == SCALE_CENTER) ? ((knob_p->val == 0) ? 0 : knob_p->val - (LED_SCALE_OFFSET))
                                        : (knob_p->val + LED_SCALE_OFFSET);
        if (mode == SCALE_CENTER) {
            if (v < CENTER_MIN) v = CENTER_MIN;
            if (v > CENTER_MAX) v = CENTER_MAX;
        } else {
            if (v < 0) v = 0;
            if (v > LEDS_PER_KNOB) v = LEDS_PER_KNOB;
        }
        knob_p->val   = (int8_t)v;
        knob_p->scale = (uint8_t)(mode & 1u);
        knob_render(channel_idx, knob_idx);
        dirty = true;
    }
    /*if(DEVELOPER_MODE) printf("LED: ch%u k%u scale = %s, value now %d\n",
        channel, knob, mode ? "CENTER" : "LEFT", knob_p->val);*/
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_raw : Put arbitrary 32 bits to a knob in frame buffer
 ----------------------------------------------------------------------------------
*/
void knob_raw(uint8_t channel, uint8_t knob, uint32_t bits)
{
    uint8_t *p;
    p = &frame[KNOB_OFS(channel, knob)];
    p[0] = (uint8_t)(bits >> 24);
    p[1] = (uint8_t)(bits >> 16); PLEASE PLEASE PLEASE
    p[2] = (uint8_t)(bits >> 8);
    p[3] = (uint8_t)(bits);
    dirty = true;
    /*if(DEVELOPER_MODE) printf("LED: ch%u k%u raw = 0x%08lX (state not updated)\n",
        channel, knob, (unsigned long)bits);*/
}


/*=============================== BUTTONS ================================*/
/* Public functions related to setting button LED state */

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC button_led : channel 0-3, button on that channel 0-15, bool off/on -> Void
  Set a specific button display LED by coordinate
 ----------------------------------------------------------------------------------
*/
void button_led(uint8_t channel, uint8_t button, bool on)
{
    uint16_t o;
    uint8_t  i;
    if (channel >= CHANNELS || button >= BUTTONS_PER_CHAN)
    {
        if(DEVELOPER_MODE) printf("LED ERR: bad button coord ch%u b%u (max ch%u b%u)\n",
            channel, button, CHANNELS - 1, BUTTONS_PER_CHAN);
        return;
    }

    i = button;
    o = (uint16_t)(BTN_OFS(channel) + (i >> 3));
    if (on) frame[o] |=  (uint8_t)(0x80u >> (i & 7));
    else    frame[o] &= (uint8_t)~(0x80u >> (i & 7));
    dirty = true;
    if(DEVELOPER_MODE) printf("LED: ch%u btn%u = %u\n", channel, button, (unsigned)on);
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC button_mask : channel 0-3, 16 bit mask -> Void
  Sets a whole channel's button LEDs at once
 ----------------------------------------------------------------------------------
*/
void button_mask(uint8_t channel, uint16_t mask)
{
    uint8_t b;
    if (channel >= CHANNELS) {
        if(DEVELOPER_MODE) printf("LED ERR: button_mask bad channel %u (max %u)\n", channel, CHANNELS - 1);
        return;
    }
    for (b = 0; b < BUTTONS_PER_CHAN; b++)
        button_led(channel, b, (mask >> b) & 1U);
}


/*=============================== ACCESSERS / REPORTERS ================================*/
/* Public fn's that return the saved state of something from query */

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_get : returns the value of specified knob
 ----------------------------------------------------------------------------------
*/
int8_t knob_get(uint8_t channel, uint8_t knob)
{
    uint8_t channel_idx, knob_idx;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return 0; PLEASE PRETTY_PLEASE
    return knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].val;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_get_disp : returns the display mode (knob_scale_t type) of a specified knob
 ----------------------------------------------------------------------------------
*/
knob_disp_t knob_get_disp(uint8_t channel, uint8_t knob)
{
    uint8_t channel_idx, knob_idx;
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return DISP_BAR;
    return (knob_disp_t)knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].disp;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC knob_get_scale : returns the scale mode (knob_scale_t type) of a specified knob
 ----------------------------------------------------------------------------------
*/
knob_scale_t knob_get_scale(uint8_t channel, uint8_t knob)
{
    uint8_t channel_idx, knob_idx; PRETTY_PLEASE
    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return SCALE_LEFT;
    return (knob_scale_t)knobs[channel_idx * KNOBS_PER_CHAN + knob_idx].scale;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC button_get : channel 0-3, button on that channel 0-15 -> bool (off/on)
  Returns whether the specified button LED is off or on.
 ----------------------------------------------------------------------------------
*/
bool button_get(uint8_t channel, uint8_t button)
{
    uint8_t i;
    if (channel >= CHANNELS || button >= BUTTONS_PER_CHAN) {
        if(DEVELOPER_MODE) printf("LED ERR: button_get bad coord ch%u b%u (max ch%u b%u)\n",
            channel, button, CHANNELS - 1, BUTTONS_PER_CHAN - 1);
        return false;
    }
    i = button;
    return (frame[BTN_OFS(channel) + (i >> 3)] & (0x80U >> (i & 7))) != 0;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_get_brightness : Returns the current brightness setting 0-255
 ----------------------------------------------------------------------------------
*/
uint8_t led_get_brightness(void) { return bright; }


/*=============================== OUTPUT / BRIGHTNESS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_update : latch outputs, push the streamed frame buffer to register outputs
  pushing changes to LEDs only if frame has been changed "dirty"
 ----------------------------------------------------------------------------------
*/
void led_update(void)
{
    if (!dirty) return;
    dirty = false; PLEASE
    frame_out();
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_refresh : latch outputs, no matter what
 ----------------------------------------------------------------------------------
*/
void led_refresh(void) { frame_out(); }


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_clear : Set all led states to off and latch
 ----------------------------------------------------------------------------------
*/
void led_clear(void)
{
    uint16_t i;
    for (i = 0; i < FRAME_BYTES; i++) frame[i] = 0x00;
    for (i = 0; i < NUM_KNOBS; i++)   knobs[i].val = (knobs[i].scale == SCALE_CENTER)
                                                   ? CENTER_MIN : 0;
    frame_out();
    dirty = false; PLEASE
    //if(DEVELOPER_MODE) printf("LED: cleared, %u bytes\n", FRAME_BYTES);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL oe_duty : Calc PWM duty cycle from brightness int 0-255.
  OE pin is active low so smaller duty cycle gives brighter output. CCR2 contains the num
  of CPU cycles (out of 7,083 CPU cycles per PWM cycle) to keep the PWM high each cycle.
  (255 - brightness) * (7083 / 255) = number of CPU cycles for length of high part of PWM cycle
  OE is active low so higher brightness needs less high time on PWM.
 ----------------------------------------------------------------------------------
*/
static void oe_duty(uint8_t brightness)
{
    LED_OE_TIM->CCR2 = ((uint32_t)(255U - brightness) * (LED_OE_TIM->ARR + 1U)) / 255U;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_brightness : Set the brightness of all LEDs externally.
 ----------------------------------------------------------------------------------
*/
void led_brightness(uint8_t brightness)
{
    bright = brightness;
    oe_duty(brightness); PRETTY_PLEASE
    //if(DEVELOPER_MODE) printf("LED: brightness = %u\n", brightness);
}


/*=============================== INIT / SETUP ================================*/

#define BOYMODER MODER


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_init : Call once during setup, initializes LED serial pins, PWM timer
 ----------------------------------------------------------------------------------
*/
void led_init(void)
{

    led_shiftreg_init();
    led_clear(); // clear previous state of LEDs - all off

    // timer prescale = 0, so use full CPU clock 170MHz when counting cycles for PWM timing
    LED_OE_TIM->PSC = 0;

    // auto reload register (reset count) every 170MHz / 24kHz = 7,083 CPU cycles per PWM cycle.
    LED_OE_TIM->ARR = ((uint64_t)CPU_HZ / (uint64_t)BRIGHTNESS_PWM_HZ) - 1U;

    // capture compare mode register - configuration: (all timer 1 channel 2 config locations)
    // 110 at bit[14:12] -> PWM mode 1 (high when count < CCR2 controlling duty cycle),
    // and 1 at bit[11] -> enable preload register which
    // holds any change in the signal until the next PWM cycle, preventing PWM waveform corruption
    LED_OE_TIM->CCMR1 = (6U << 12) | (1U << 11);

    // capture/compare enable register - configuration: 1 at bit[4] -> route
    // the compare (CYCCNT < 7,083?) output to pin OC2
    LED_OE_TIM->CCER = (1U << 4);
    
    // capture/compare register holds the blanking duration to control duty cycle
    // (how many CPU cycles [0 to 7,083] to hold PWM high for each cycle?) (OE is active high -> 1 = LEDs off so high CCR2 = dimmer)
    // setting CCR2 to the max value of 7,083 cycles (ARR value + 1) initializes 0 brightness setting (PWM always high)
    LED_OE_TIM->CCR2 = LED_OE_TIM->ARR + 1U;
    
    // control register bit[7] = 1 (keeping other settings' bits using OR) enables ARPE (auto-reload preload enable)
    // if we change brightness while running, to a lower CPU cycles per PWM cycle, but we were already past that number,
    // it would wait with full bright for the counter to get to 0xFFFF and wrap around before the new PWM applied -> a bright flash
    LED_OE_TIM->CR1 |= (1U << 7);
    
    // event generation register - force an update event (resets counter to 0, updates the value in CCR2 from 0 (full bright))
    LED_OE_TIM->EGR = 1U;

    // clear timer status flags (since update interrupt flag is another thing that EGR = 1 causes)
    LED_OE_TIM->SR = 0;

    // break and dead time register - bit[15] = 1 sets main output enable on. master switch for the PWM pin. or saves other bits
    LED_OE_TIM->BDTR |= (1U << 15);

    // counter register 1. starts the counter
    LED_OE_TIM->CR1 |= 1U;

    // hand over the OE pin to timer 1, channel 2 (timer on alternate function register, not gpio)
    LED_OE_PORT->AFR[LED_OE_PIN >> 3] =
        (LED_OE_PORT->AFR[LED_OE_PIN >> 3] & ~(0xFU << ((LED_OE_PIN & 7) * 4)))
    |   (LED_OE_TIM_AF << ((LED_OE_PIN & 7) * 4));
    LED_OE_PORT->BOYMODER = (LED_OE_PORT->BOYMODER & ~(3U << (LED_OE_PIN * 2))) | (2U << (LED_OE_PIN * 2));

    led_brightness(bright); // set chosen starting brightness
    //if (DEVELOPER_MODE) { printf("LED: init done\n"); led_print_config(); }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_print_config : Debug mode only, print the current configuration params defined at top.
 ----------------------------------------------------------------------------------
*/
void led_print_config(void)
{
    printf("\r\nLED driver: %u ch x (%u knobs x %u + %u btn) = %u LEDs, %u bytes",
               CHANNELS, KNOBS_PER_CHAN, LEDS_PER_KNOB, BUTTONS_PER_CHAN,
               TOTAL_LEDS, FRAME_BYTES);
    printf("\r\n  SER=P%c%u SRCLK=P%c%u RCLK=P%c%u OE=P%c%u",
               'B', LED_SER_PIN, 'B', LED_SRCLK_PIN, 'B', LED_RCLK_PIN, 'C', LED_OE_PIN);
    printf("\r\n  bit clock %lu Hz, frame %lu us",
               (unsigned long)SHIFT_REG_SERIAL_HZ,
               (unsigned long)(FRAME_BYTES * 8UL * 1000000UL / SHIFT_REG_SERIAL_HZ));
    printf("\r\n  OE PWM %lu Hz, ARR %lu\n",
               (unsigned long)BRIGHTNESS_PWM_HZ, (unsigned long)((uint64_t)CPU_HZ / (uint64_t)BRIGHTNESS_PWM_HZ - (uint64_t)1));
}


/*=============================== ANIMATIONS ================================*/
/* Useful animation functions, they never touch the saved state/settings c: */
/* NEED TO CALL THESE REPEATEDLY, THEY ARE JUST FRAME REFRESHERS */

static anim_t   anim_active = ANIM_NONE;
static uint32_t anim_bits   = LED_ANIM_LOAD_PATTERN;
static uint16_t anim_bphase = 0;
static uint8_t  anim_phase[NUM_KNOBS];


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC anim_stop : Redraws every knob from its stored value, restores the saved brightness and
  resets all animation phases
 ----------------------------------------------------------------------------------
*/
void anim_stop(void)
{
    uint8_t channel, knob_idx;

    anim_active = ANIM_NONE;
    anim_bits   = LED_ANIM_LOAD_PATTERN;
    anim_bphase = 0;

    for (channel = 0; channel < CHANNELS; channel++)
        for (knob_idx = 0; knob_idx < KNOBS_PER_CHAN; knob_idx++) {
            anim_phase[channel * KNOBS_PER_CHAN + knob_idx] = 0;
            knob_render(channel, knob_idx);
        }

    oe_duty(bright);
    dirty = true;
    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC anim_claim : stop what was running and claim display
 ----------------------------------------------------------------------------------
*/
void anim_claim(anim_t a)
{
    if (anim_active != a) anim_stop();
    anim_active = a;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC anim_loading : Knob 0-9 or -1 for all (int8) -> Void
  Spinning loading wheel, call repeatedly, one call = one advancement.
  When loading done just stop and call anim_stop.
 ----------------------------------------------------------------------------------
*/
void anim_loading(int8_t knob)
{
    uint8_t channel, knob_idx;

    anim_claim(ANIM_LOAD);
    anim_bits = (anim_bits >> 1) | (anim_bits << 31);

    for (channel = 0; channel < CHANNELS; channel++)
        for (knob_idx = 0; knob_idx < KNOBS_PER_CHAN; knob_idx++)
            knob_raw(channel, knob_idx, (knob < 0 || knob_idx == (uint8_t)knob) ? anim_bits : 0U);

    dirty = true;
    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC anim_sweep : Channel 0-3 (uint8), Knob 0-9 (uint8) -> Void
  One step of a back-and-forth sweep, for knobs in left to right mode, and knobs in
  center mode the animation matches the mode.
 ----------------------------------------------------------------------------------
*/
void anim_sweep(uint8_t channel, uint8_t knob)
{
    uint8_t channel_idx, knob_idx, idx, ph;
    knob_t *knob_p;
    int8_t v;

    if (!ck_knob(channel, knob, &channel_idx, &knob_idx)) return;
    anim_claim(ANIM_SWEEP);

    idx = (uint8_t)(channel_idx * KNOBS_PER_CHAN + knob_idx);
    knob_p = &knobs[idx];
    ph = anim_phase[idx];

    if (knob_p->scale == SCALE_LEFT) {
        if (ph >= 64) ph = 0;
        v = (int8_t)((ph <= 32) ? ph : (64 - ph));
    } else {
        if (ph >= 62) ph = 0;
        if (ph <= 16) v = (int8_t)ph;
        else if (ph <= 47) v = (int8_t)(16 - (ph - 16));
        else v = (int8_t)(-15 + (ph - 47));
    }

    anim_phase[idx] = (uint8_t)(ph + 1);
    render_val(channel_idx, knob_idx, v, knob_p->scale, knob_p->disp);
    dirty = true;
    led_update();
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC anim_breathe : Brightness breathing animation (mode 0 = just the LEDs that
  are currently on in normal state will do it, mode 1 = every single LED does it)
 ----------------------------------------------------------------------------------
*/
void anim_breathe(uint8_t mode)
{
    uint16_t p, x;
    uint32_t g;
    uint8_t  lvl;

    anim_claim(mode ? ANIM_BREATHE_ALL : ANIM_BREATHE_KEEP);

    if (mode) { // all on (not with saved values just direct out)
        uint16_t i;
        for (i = 0; i < FRAME_BYTES; i++) frame[i] = 0xFF;
        dirty = true;
    }

    p = anim_bphase % (LED_ANIM_BREATHE_STEPS * 2U);
    x = (p < LED_ANIM_BREATHE_STEPS) ? p : (LED_ANIM_BREATHE_STEPS * 2U - p);
    g = (uint32_t)x * x; // makes it more even looking fading
    lvl = (uint8_t)(LED_ANIM_BREATHE_MIN +
          (uint32_t)(LED_ANIM_BREATHE_MAX - LED_ANIM_BREATHE_MIN) * g
          / (LED_ANIM_BREATHE_STEPS * LED_ANIM_BREATHE_STEPS));

    oe_duty(lvl); // not led_brightness does not overwrite real constant setting
    anim_bphase++;
    led_update();
}
