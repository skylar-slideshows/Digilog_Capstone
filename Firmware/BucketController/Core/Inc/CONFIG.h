/**
  **********************************************************************************
  * MASTER CONFIGURATION FILE
  **********************************************************************************
  * @file CONFIG.h
  * @brief A significant effort has been made to place all configuration parameters here.
  *        There are two full configuration setups. Development and release.
  *        Development may be changed as needed. Please try to avoid changing the release config.
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

#include "stm32g474xx.h"
#include "hardware_drivers/i2c_driver.h"

#ifndef CONFIG_H
#define CONFIG_H

#ifndef DEVELOPER_MODE
    /************************************************************************************/
    #define DEVELOPER_MODE 1 // SET CONFIGURATION: 1 = Developer mode, 0 = Release mode
    /************************************************************************************/
#endif



#if DEVELOPER_MODE

    /*=============================== CLOCKS ================================*/
    /** @brief Set clock rates below*/
    #define CPU_HZ              (uint64_t)SystemCoreClock // nominal 170MHz, but is set in CubeMX.
                                                          // this value is just used by functions to calc their own clocks   
    #define SHIFT_REG_SERIAL_HZ (uint64_t)480000U         // Frequency of serial output to shift registers
    #define BRIGHTNESS_PWM_HZ   (uint64_t) 24000U         // To control brightness of LEDs and LCD backlights, PWM frequency


    /*=============================== CONSOLE SIZE ================================*/
    /** @brief Set number of knobs, channels, buttons below*/
    #define CHANNELS                4
    #define KNOBS_PER_CHAN         10
    #define BUTTONS_PER_CHAN       16


    /*=============================== I2C ================================*/
    /** @brief Set I2C bus parameters for the set of identical i2c buses */
    #define I2C_BUSES          ((I2C_TypeDef *[]){ I2C1, I2C2, I2C3, I2C4 }) // which specific i2c buses to initialize?, should match channels

    // chip addresses: (present identically in each bus declared above)
    #define MCP23017_ADDRS     ((uint8_t[]){ 0x20, 0x21, 0x22 })
    #define MCP4728_ADDRS      ((uint8_t[]){ 0x60, 0x61, 0x62, 0x63, 0x64 })

    // datarates (see/understand top of i2c_driver.h before touching!)
    #define I2C_SLOT_PERIOD_US     667   // maximum time of one slot (667us = 1.5kHz)
    #define I2C_MAX_TRANSFER_LEN     8   // (bytes) as long as nbytes is <255 for each transfer, it can be done at once and does not need reload mode
    #define I2C_MAX_TRANSFER_SLOT    5   // max num descriptors on one slot (slot_t.x[4] max).
                                         // (see top of i2c_driver.h) most slots have 2: 0x20, 0x21 and 2 dacs, but every 15th has 0x22
    #define I2C_SLOTS_PER_SFRAME    15   // superframe length (how many I2C frames per superframe, at 1.5khz = 10ms)

    // timeouts (how long to try busy bus until declare fail?)
    #define RESTART_US               4   // 4 microsec restart time
    #define TIMEOUT_US            2000   // 2ms waiting for transfer to complete


    /*=============================== MCP4728 DAC INITIALIZATION ================================*/

    #define DACADDR_DATA_PORT          GPIOA               // serial, sck on same port for timing
    #define DACADDR_SER_PIN            12                  // DAC_INIT_Data = PA12
    #define DACADDR_SRCLK_PIN          11                  // DAC_INIT_Clock = PA11
    #define DACADDR_LATCH_PORT         GPIOA
    #define DACADDR_RCLK_PIN           7                  // DAC_INIT_Data = PC13


    /*=============================== SPI 1 ================================*/
    /** @brief Settings for serial peripheral interface that communicates w/ master controller */


    /*=============================== LCD DISPLAYS / SPI 2 ================================*/
    /** @brief LCD settings and SPI 2 (LCDs only) settings */
    #define LCD_LENGTH_PX 240
    #define LCD_WIDTH_PX  240
    #define LCD_BITS_RED    6
    #define LCD_BITS_GRN    5
    #define LCD_BITS_BLU    6


    /*=============================== ADDTL / SPI 3 ================================*/
    /** @brief Additional chips (compressor ADCs and MCP23S17 for CS/DC pins of displays) and their SPI bus settings */


    /*=============================== LEDS (KNOB RINGS AND BUTTON LEDS) ================================*/
    /** @brief Set parameters for LED knob rings and button leds below */

    // Pins
    #define LED_DATA_PORT          GPIOB               // serial, sck, latch all on same port for timing
    #define LED_SER_PIN            11                  // LED_Data = PB11
    #define LED_SRCLK_PIN          12                  // LED_Clock = PB12
    #define LED_RCLK_PIN           14                  // LED_Latch = PB14
    #define LED_DATA_PORT_EN       RCC_AHB2ENR_GPIOBEN // enable gpio clock for port above

    #define LED_OE_PORT            GPIOC               // port for output enable pwm brightness ctrl pin
    #define LED_OE_PIN             1                   // LED_PWM = PC1
    #define LED_OE_PORT_EN         RCC_AHB2ENR_GPIOCEN // enable gpio clock for port above
    #define LED_OE_TIM             TIM1                // which onboard timer? channel 2 bit locations set in led_driver.c
    #define LED_OE_TIM_EN          RCC_APB2ENR_TIM1EN  // enable for timer above
    #define LED_OE_TIM_AF          2U                  // select tim1 chan 2 (move if LED_PWN change pins)

    // LEDS geometry
    #define LEDS_PER_KNOB          32
    #define CENTER_LED             15
    #define LED_SCALE_OFFSET       16

    // animations
    #define LED_ANIM_LOAD_PATTERN  0xE0E0E0E0U
    #define LED_ANIM_BREATHE_MIN   1U // dimmest
    #define LED_ANIM_BREATHE_MAX   128U // brightest
    #define LED_ANIM_BREATHE_STEPS 64U // num steps between these values


#else // please do not edit the below configuration


    /*=============================== CLOCKS ================================*/
    /** @brief Set clock rates below*/
    #define CPU_HZ             (uint64_t)SystemCoreClock // nominal 170MHz, but is set in CubeMX.
                                                         // this value is just used by functions to calc their own clocks   
    #define LED_SERIAL_HZ      (uint64_t)480000U         // Frequency of serial output to LED shift registers
    #define BRIGHTNESS_PWM_HZ  (uint64_t) 24000U         // To control brightness of LEDs and LCD backlights, PWM frequency


    /*=============================== CONSOLE SIZE ================================*/
    /** @brief Set number of knobs, channels, buttons below*/
    #define CHANNELS                4
    #define KNOBS_PER_CHAN         10
    #define BUTTONS_PER_CHAN       16


    /*=============================== I2C ================================*/
    /** @brief Set I2C bus parameters for the set of identical i2c buses */
    #define I2C_BUSES          ((I2C_TypeDef *[]){ I2C1, I2C2, I2C3, I2C4 }) // which specific i2c buses to initialize?, should match channels

    // chip addresses: (present identically in each bus declared above)
    #define MCP23017_ADDRS     ((uint8_t[]){ 0x20, 0x21, 0x22 })
    #define MCP4728_ADDRS      ((uint8_t[]){ 0x60, 0x61, 0x62, 0x63, 0x64 })

    // datarates (see/understand top of i2c_driver.h before touching!)
    #define I2C_SLOT_PERIOD_US     667   // maximum time of one slot (667us = 1.5kHz)
    #define I2C_MAX_TRANSFER_LEN     8   // (bytes) as long as nbytes is <255 for each transfer, it can be done at once and does not need reload mode
    #define I2C_MAX_TRANSFER_SLOT    5   // max num descriptors on one slot (slot_t.x[4] max).
                                         // (see top of i2c_driver.h) most slots have 2: 0x20, 0x21 and 2 dacs, but every 15th has 0x22
    #define I2C_SLOTS_PER_SFRAME    15   // superframe length (how many I2C frames per superframe, at 1.5khz = 10ms)

    // timeouts (how long to try busy bus until declare fail?)
    #define RESTART_US               4   // 4 microsec restart time
    #define TIMEOUT_US            2000   // 2ms waiting for transfer to complete


    /*=============================== SPI 1 ================================*/
    /** @brief Settings for serial peripheral interface that communicates w/ master controller */


    /*=============================== LCD DISPLAYS / SPI 2 ================================*/
    /** @brief LCD settings and SPI 2 (LCDs only) settings */
    #define LCD_LENGTH_PX 240
    #define LCD_WIDTH_PX  240
    #define LCD_BITS_RED    6
    #define LCD_BITS_GRN    5
    #define LCD_BITS_BLU    6


    /*=============================== ADDTL / SPI 3 ================================*/
    /** @brief Additional chips (compressor ADCs and MCP23S17 for CS/DC pins of displays) and their SPI bus settings */


    /*=============================== LEDS (KNOB RINGS AND BUTTON LEDS) ================================*/
    /** @brief Set parameters for LED knob rings and button leds below */

    // Pins
    #define LED_DATA_PORT          GPIOB               // serial, sck, latch all on same port for timing
    #define LED_SER_PIN            11                  // LED_Data = PB11
    #define LED_SRCLK_PIN          12                  // LED_Clock = PB12
    #define LED_RCLK_PIN           14                  // LED_Latch = PB14
    #define LED_DATA_PORT_EN       RCC_AHB2ENR_GPIOBEN // enable gpio clock for port above

    #define LED_OE_PORT            GPIOC               // port for output enable pwm brightness ctrl pin
    #define LED_OE_PIN             1                   // LED_PWM = PC1
    #define LED_OE_PORT_EN         RCC_AHB2ENR_GPIOCEN // enable gpio clock for port above
    #define LED_OE_TIM             TIM1                // which onboard timer? channel 2 bit locations set in led_driver.c
    #define LED_OE_TIM_EN          RCC_APB2ENR_TIM1EN  // enable for timer above
    #define LED_OE_TIM_AF          2U                  // select tim1 chan 2 (move if LED_PWN change pins)

    // LEDS geometry
    #define LEDS_PER_KNOB          32
    #define CENTER_LED             15
    #define LED_SCALE_OFFSET       16

    // animations
    #define LED_ANIM_LOAD_PATTERN  0xE0E0E0E0U
    #define LED_ANIM_BREATHE_MIN   1U // dimmest
    #define LED_ANIM_BREATHE_MAX   128U // brightest
    #define LED_ANIM_BREATHE_STEPS 64U // num steps between these values


#endif

#endif // CONFIG_H