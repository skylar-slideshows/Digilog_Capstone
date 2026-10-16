/**
  **********************************************************************************
  * MASTER CONFIGURATION FILE
  **********************************************************************************
  * @file CONFIG.h
  * @brief All configuration parameters live here, except pin labels. Everything is shared
  *        between developer and release builds except the few values that genuinely differ
  *        (see the end of this file).
  *
  *        MIGRATING TO A NEW PIN LAYOUT / MCU: regenerate with CubeMX, then
  *         1. update the HARDWARE MAPPING section below (which peripheral instances and
  *            handles are used);
  *         2. fix any renamed pin labels. Pins are referenced directly by their CubeMX
  *            labels (e.g. F0_MA_Pin / F0_MA_GPIO_Port, GPIO_PIN_x bitmasks) in:
  *            74hc595.c, fader_control.c, and the channel table in i2c_driver.c.
  *
  *        Macros below are only kept where a compile-time constant is required (array sizes,
  *        #if, static initializers); everything else is a typed const.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-08-25
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

#ifndef CONFIG_H
#define CONFIG_H

#include "stm32g474xx.h"
#include "main.h" // CubeMX labels (<Name>_Pin, <Name>_GPIO_Port) and peripheral names

#ifndef DEVELOPER_MODE
    /************************************************************************************/
    #define DEVELOPER_MODE 1 // SET CONFIGURATION: 1 = Developer mode, 0 = Release mode
    /************************************************************************************/
#endif


/*=============================== CONSOLE SIZE ================================*/
/** @brief Set number of knobs, channels, buttons below (macros: used for array sizes) */
#define CHANNELS                4
#define KNOBS_PER_CHAN         10
#define BUTTONS_PER_CHAN       16


/*============================== HARDWARE MAPPING ================================*/
/** @brief Which peripheral instances / handles / channels the firmware uses. Repoint these
 *         after regenerating in CubeMX. (Pin labels are referenced directly, see top of file.) */

/*------------------------------ I2C (one bus per channel) ------------------------------*/
/** AF is only used to hand the pins back after bit-banging (MCP4728 address programming).
 *  It must match what CubeMX generated in stm32g4xx_hal_msp.c for that peripheral. */
#define CHANNEL0_I2C_BUS       I2C1
#define CHANNEL0_I2C_AF        GPIO_AF4_I2C1
#define CHANNEL1_I2C_BUS       I2C2
#define CHANNEL1_I2C_AF        GPIO_AF4_I2C2
#define CHANNEL2_I2C_BUS       I2C3
#define CHANNEL2_I2C_AF        GPIO_AF8_I2C3
#define CHANNEL3_I2C_BUS       I2C4
#define CHANNEL3_I2C_AF        GPIO_AF8_I2C4

/*------------------------------ LEDS ------------------------------*/
/** LED data goes out over an SPI peripheral with DMA. Brightness is a PWM on the shift
 *  registers' (active low) output enable, driven by a timer channel. */
#define LED_SPI_HANDLE         hspi2
#define LED_SPI_INSTANCE       SPI2
#define LED_OE_TIM_HANDLE      htim1
#define LED_OE_TIM_CHAN        TIM_CHANNEL_2

/*------------------------------ FADERS ------------------------------*/
/** Fader n belongs to channel n. FADER_ADC must have its regular conversions ranked in fader
 *  order (rank 1 = fader 0 ... rank 4 = fader 3), see poll_faders(). */
#define FADER_ADC              hadc2

#define FADER0_TIM_HANDLE      htim2
#define FADER0_A_TIM_CHAN      TIM_CHANNEL_1
#define FADER0_B_TIM_CHAN      TIM_CHANNEL_2

#define FADER1_TIM_HANDLE      htim2
#define FADER1_A_TIM_CHAN      TIM_CHANNEL_3
#define FADER1_B_TIM_CHAN      TIM_CHANNEL_4

#define FADER2_TIM_HANDLE      htim3
#define FADER2_A_TIM_CHAN      TIM_CHANNEL_1
#define FADER2_B_TIM_CHAN      TIM_CHANNEL_2

#define FADER3_TIM_HANDLE      htim3
#define FADER3_A_TIM_CHAN      TIM_CHANNEL_3
#define FADER3_B_TIM_CHAN      TIM_CHANNEL_4

/*------------------------------ REMOTE UART (USB debug + remote app) ------------------------------*/
#define REMOTE_UART_HANDLE     huart2           // HAL handle, used for printf
#define REMOTE_UART            USART2           // register access by the command handler
#define REMOTE_UART_IRQn       USART2_IRQn
#define REMOTE_UART_IRQHandler USART2_IRQHandler


/*=============================== CLOCKS ================================*/
/** @brief Set clock rates below. The CPU clock itself (nominal 170MHz) is set in CubeMX and
 *         read at runtime from SystemCoreClock; functions use it to calc their own clocks. */
static const uint32_t SHIFT_REG_SERIAL_HZ = 480000U; // Frequency of serial output to shift registers
static const uint32_t BRIGHTNESS_PWM_HZ   =  24000U; // To control brightness of LEDs and LCD backlights, PWM frequency


/*=============================== I2C ================================*/
/** @brief Set I2C bus parameters (the buses themselves are in HARDWARE MAPPING above) */

// chip addresses: (present identically in each bus)
static const uint8_t MCP23017_ADDRS[] = { 0x20, 0x21, 0x22 };
static const uint8_t MCP4728_ADDRS[]  = { 0x60, 0x61, 0x62, 0x63, 0x64 };

// datarates (see/understand top of i2c_driver.h before touching!)
static const uint32_t I2C_SLOT_PERIOD_US   = 667; // maximum time of one slot (667us = 1.5kHz)
static const uint8_t  I2C_MAX_TRANSFER_LEN =   8; // (bytes) as long as nbytes is <255 for each transfer, it can be done at once and does not need reload mode
static const uint8_t  I2C_MAX_TRANSFER_SLOT =  5; // max num descriptors on one slot (slot_t.x[4] max).
                                                  // (see top of i2c_driver.h) most slots have 2: 0x20, 0x21 and 2 dacs, but every 15th has 0x22
static const uint8_t  I2C_SLOTS_PER_SFRAME = 15;  // superframe length (how many I2C frames per superframe, at 1.5khz = 10ms)

// timeouts (how long to try busy bus until declare fail?)
static const uint32_t RESTART_US =   4; // 4 microsec restart time
static const uint32_t TIMEOUT_US = 500; // 500us waiting for transfer to complete


/*=============================== SPI 1 ================================*/
/** @brief Settings for serial peripheral interface that communicates w/ master controller */


/*=============================== LCD DISPLAYS / SPI 2 ================================*/
/** @brief LCD settings and SPI 2 (LCDs only) settings */
static const uint16_t LCD_LENGTH_PX = 240;
static const uint16_t LCD_WIDTH_PX  = 240;
static const uint8_t  LCD_BITS_RED  = 6;
static const uint8_t  LCD_BITS_GRN  = 5;
static const uint8_t  LCD_BITS_BLU  = 6;


/*=============================== ADDTL / SPI 3 ================================*/
/** @brief Additional chips (compressor ADCs and MCP23S17 for CS/DC pins of displays) and their SPI bus settings */


/*=============================== LED GEOMETRY ================================*/
/** @brief Set parameters for LED knob rings and button leds below */

#define LEDS_PER_KNOB          32 // macro: used for array sizes (LED_CHAIN_BITS)
static const uint8_t CENTER_LED       = 16;
static const uint8_t LED_SCALE_OFFSET = 17;

// animations
#define LED_ANIM_LOAD_PATTERN  0xE0E0E0E0U // macro: used in a static initializer
static const uint8_t  LED_ANIM_BREATHE_MIN   =   1U; // dimmest
static const uint8_t  LED_ANIM_BREATHE_MAX   = 128U; // brightest
static const uint32_t LED_ANIM_BREATHE_STEPS =  64U; // num steps between these values


/*=============================== DEVELOPER / RELEASE DIFFERENCES ================================*/
/** @brief The only settings that differ between the two modes. Please avoid changing the release value. */

#if DEVELOPER_MODE
    #define LED_CHAIN_BITS       72 // 9x 74HC595 populated on the test board (macro: array size)
#else
    #define LED_CHAIN_BITS     1344
#endif

#endif // CONFIG_H
