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
  * @author Skylar Denno (denno.o@northeastern.edu) Darya Petrova (petrov.da@northeastern.edu)
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

/*------------------------------ COMPRESSOR ------------------------------*/
/** 1 once CubeMX has TIM6 triggering ADC4 with DMA (steps at the top of compressor.c). At 0 the
 *  compressor DACs and op-amps still start, holding every channel at 0 dB (safe for comp cards), but
 *  nothing compresses. */
#define COMP_LOOP_ENABLED 0
#define COMP_SAMPLE_HZ    32000     // nominal; the real rate is read from TIM6
#define COMP_DAC_VREF     3.3f      // VREF+ of the STM32's ADCs and DACs, volts
#define COMP_R7_OHMS      48700.0f  // comp card scaler input resistor: 48.7k as built (the card note's
                                    // 3.3 V value is 78.7k; set this to match if it's changed)

/*------------------------------ FADERS ------------------------------*/
/** 0 while the faders are unplugged: they're ignored completely (not read, motors not driven), since
 *  their open inputs read random positions and touches. Every channel's level then stays at 0 dB
 *  (its fader at the 0 dB mark). Set to 1 once the faders are connected. */
#define FADERS_CONNECTED 0

/** Fader position of 0 dB (cv_mapping.pdf section 18): the level channels sit at while FADERS_CONNECTED is 0 */
#define FADER_0DB_POSITION 44380U

/** What each fader is wired to (fader n belongs to channel n+1). Since the pin remap of November 2026 a
 *  fader's two motor outputs can be on different timers (TIM2 and TIM3 run the same PWM), and the
 *  positions come from two ADCs. Touch inputs are the CubeMX labels F0_TOUCH ... F3_TOUCH.
 *
 *  fader   motor A          motor B          position          touch
 *    0     PA5  TIM2 CH1    PB10 TIM2 CH3    PD9  ADC5 IN13    PE15
 *    1     PA6  TIM3 CH1    PA7  TIM3 CH2    PD8  ADC5 IN12    PB2
 *    2     PB7  TIM3 CH4    PD6  TIM2 CH4    PC3  ADC2 IN9     PE6
 *    3     PE4  TIM3 CH3    PA1  TIM2 CH2    PC0  ADC2 IN6     PE5   */
#define FADER0_A_TIM      htim2
#define FADER0_A_CHAN     TIM_CHANNEL_1
#define FADER0_B_TIM      htim2
#define FADER0_B_CHAN     TIM_CHANNEL_3
#define FADER0_ADC        hadc5
#define FADER0_ADC_CHAN   ADC_CHANNEL_13

#define FADER1_A_TIM      htim3
#define FADER1_A_CHAN     TIM_CHANNEL_1
#define FADER1_B_TIM      htim3
#define FADER1_B_CHAN     TIM_CHANNEL_2
#define FADER1_ADC        hadc5
#define FADER1_ADC_CHAN   ADC_CHANNEL_12

#define FADER2_A_TIM      htim3
#define FADER2_A_CHAN     TIM_CHANNEL_4
#define FADER2_B_TIM      htim2
#define FADER2_B_CHAN     TIM_CHANNEL_4
#define FADER2_ADC        hadc2
#define FADER2_ADC_CHAN   ADC_CHANNEL_9

#define FADER3_A_TIM      htim3
#define FADER3_A_CHAN     TIM_CHANNEL_3
#define FADER3_B_TIM      htim2
#define FADER3_B_CHAN     TIM_CHANNEL_2
#define FADER3_ADC        hadc2
#define FADER3_ADC_CHAN   ADC_CHANNEL_6

#define FADER_ADC_SAMPLETIME ADC_SAMPLETIME_47CYCLES_5 // fader pots: a longer sample settles better

/*------------------------------ CV DACs ------------------------------*/
/** The I2C bus each channel's control-voltage DACs (its five MCP4728s) are on. A channel's MCP23017s are on
 *  the bus with its own number (I2C1 = channel 1 ...), but since the November 2026 remap the DACs aren't:
 *  I2C1 carries channel 4's CVs, I2C2 channel 1's, I2C3 channel 3's, I2C4 channel 2's. */
#define CV_DAC_BUS_CH1    I2C2
#define CV_DAC_BUS_CH2    I2C4
#define CV_DAC_BUS_CH3    I2C3
#define CV_DAC_BUS_CH4    I2C1

/*------------------------------ DEBUG UART (USART2, 921600) ------------------------------*/
#define REMOTE_UART_HANDLE     huart2           // HAL handle: the debug log (printf). The remote app now
                                                // reaches the bucket through the master, over SPI1.

/*------------------------------ START-UP SCREEN ------------------------------*/
/** The DIGILOG picture on the screens (boot_screen.c), until the master gives this bucket its
 *  channels or BOOT_MASTER_TIMEOUT_MS passes (then solo mode: channels 01-04, nothing over SPI, and NO MASTER
 *  in red along the bottom of the channel screens). */
#define BOOT_FADE_IN_MS         1000U
#define BOOT_FADE_OUT_MS        300U
#define BOOT_MASTER_TIMEOUT_MS  5000U   // from start-up

/*------------------------------ MASTER LINK ------------------------------*/
/** SPI1 slave to the master controller (master_link.c, BUCKET_CONTROL_SYNC.md). Each chip-select
 *  window is an exchange of up to LINK_XCHG_BYTES: commands in on MOSI, this bucket's report packet
 *  out on MISO. */
#define LINK_XCHG_BYTES 64
#define LINK_RX_RING    1024  // received bytes waiting for the link task (power of two)
#define LINK_SCAN_MS    10    // how often the panel is checked for changes to report


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
    #define LED_CHAIN_BITS       1344 // 9x 74HC595 populated on the test board (macro: array size)
#else
    #define LED_CHAIN_BITS     1344
#endif

#endif // CONFIG_H
