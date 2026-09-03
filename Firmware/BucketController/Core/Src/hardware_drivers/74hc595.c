/**
  **********************************************************************************
  * SHIFT REGISTER DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file 74hc595.c
  * @brief Skylar's driver for the 74HC595 shift registers, to run communication with
  *        the LED 74HCT595s and the address programmers for the DAC.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-08-31
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

#include "stm32g4xx.h"
#include "CONFIG.h"
#include "hardware_drivers/74hc595.h"


#define SER_SET   (1U << LED_SER_PIN)
#define SER_RST   (1U << (LED_SER_PIN + 16))
#define SRCLK_SET (1U << LED_SRCLK_PIN)
#define SRCLK_RST (1U << (LED_SRCLK_PIN + 16))
#define RCLK_SET  (1U << LED_RCLK_PIN)
#define RCLK_RST  (1U << (LED_RCLK_PIN + 16))

#define DACADDR_SER_SET   (1U << DACADDR_SER_PIN)
#define DACADDR_SER_RST   (1U << (DACADDR_SER_PIN + 16))
#define DACADDR_SRCLK_SET (1U << DACADDR_SRCLK_PIN)
#define DACADDR_SRCLK_RST (1U << (DACADDR_SRCLK_PIN + 16))
#define DACADDR_RCLK_SET  (1U << DACADDR_RCLK_PIN)
#define DACADDR_RCLK_RST  (1U << (DACADDR_RCLK_PIN + 16))

static uint32_t half_cyc; // 177 cycles at 170MHz/480kHz

#define BOYMODER MODER
#define MODE_OUT(port, pin) do { \
    (port)->BOYMODER = ((port)->BOYMODER & ~(3U << ((pin) * 2))) | (1U << ((pin) * 2)); \
    (port)->OSPEEDR |= (3U << ((pin) * 2)); \
} while (0)


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL wait_half : Count half cycle of serial LED out clock
 ----------------------------------------------------------------------------------
*/
static inline void wait_half(void)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < half_cyc) { }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC shift_bit : uint32 -> Shifts a single bit out.
 ----------------------------------------------------------------------------------
*/
void shift_bit(uint32_t bit, bool led)
{
    if(led)
    {
        LED_DATA_PORT->BSRR = (bit ? SER_SET : SER_RST) | SRCLK_RST;
        wait_half();
        LED_DATA_PORT->BSRR = SRCLK_SET;
        wait_half();
    } else {
        DACADDR_DATA_PORT->BSRR = (bit ? DACADDR_SER_SET : DACADDR_SER_RST) | DACADDR_SRCLK_RST;
        wait_half();
        DACADDR_DATA_PORT->BSRR = DACADDR_SRCLK_SET;
        wait_half();
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC shift_byte : Shift out a whole byte to the DAC LDAC shift registers
  (DAC ONLY NO LED). Goes LSB to MSB
 ----------------------------------------------------------------------------------
*/
void shift_byte(uint8_t byte)
{
    for(uint8_t i = 0; i < 8; i++)
    { shift_bit((byte >> i) & 1, 0); }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC latch_out : Pulse latch low to update register outputs
 ----------------------------------------------------------------------------------
*/
void latch_out(bool led)
{
    if(led)
    {
        LED_DATA_PORT->BSRR = SRCLK_RST;
        wait_half();
        LED_DATA_PORT->BSRR = RCLK_SET;
        wait_half();
        LED_DATA_PORT->BSRR = RCLK_RST;
    } else {
        DACADDR_DATA_PORT->BSRR = DACADDR_SRCLK_RST;
        wait_half();
        DACADDR_LATCH_PORT->BSRR = DACADDR_RCLK_SET;
        wait_half();
        DACADDR_LATCH_PORT->BSRR = DACADDR_RCLK_RST;
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_shiftreg_init : Set up pins for the shift registers for leds (serial, clock, latch)
 ----------------------------------------------------------------------------------
*/
void led_shiftreg_init(void)
{

    half_cyc = ((uint64_t)(CPU_HZ) / ((uint64_t)2UL * (uint64_t)SHIFT_REG_SERIAL_HZ));

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Debug exception & monitor ctrl register setup, trace enable
    DWT->CYCCNT = 0; // cycle counter for PWM speed
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // enable cycle counter. PWM period based on number of CPU clock cycles

    RCC->AHB2ENR |= LED_DATA_PORT_EN | LED_OE_PORT_EN; // enables GPIO pins
    RCC->APB2ENR |= LED_OE_TIM_EN; // enables Timer 1

    LED_OE_PORT->BSRR = (1U << LED_OE_PIN); // set OE high (OE active low so hide all LEDs first)
    MODE_OUT(LED_OE_PORT, LED_OE_PIN);

    MODE_OUT(LED_DATA_PORT, LED_SER_PIN); // initialize the pins needed - serial data
    MODE_OUT(LED_DATA_PORT, LED_SRCLK_PIN); // serial clock
    MODE_OUT(LED_DATA_PORT, LED_RCLK_PIN); // latch pin
    LED_DATA_PORT->BSRR = SER_RST | SRCLK_RST | RCLK_RST;
    
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC dac_shiftreg_init : Set up pins for the shift registers for MCP4728 address programming (serial, clock, latch)
 ----------------------------------------------------------------------------------
*/
void dac_shiftreg_init(void)
{

    half_cyc = ((uint64_t)(CPU_HZ) / ((uint64_t)2UL * (uint64_t)SHIFT_REG_SERIAL_HZ));

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Debug exception & monitor ctrl register setup, trace enable
    DWT->CYCCNT = 0; // cycle counter for PWM speed
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // enable cycle counter. PWM period based on number of CPU clock cycle

    MODE_OUT(DACADDR_DATA_PORT, DACADDR_SER_PIN); // initialize the pins needed - serial data
    MODE_OUT(DACADDR_DATA_PORT, DACADDR_SRCLK_PIN); // serial clock
    MODE_OUT(DACADDR_LATCH_PORT, DACADDR_RCLK_PIN); // latch pin
    DACADDR_DATA_PORT->BSRR = DACADDR_SER_RST | DACADDR_SRCLK_RST;
    DACADDR_LATCH_PORT->BSRR = DACADDR_RCLK_RST;
    
}