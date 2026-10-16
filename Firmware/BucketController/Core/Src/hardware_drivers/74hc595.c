/**
  **********************************************************************************
  * SHIFT REGISTER DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file 74hc595.c
  * @brief Skylar's driver for the 74HC595 shift registers, to run comms w/
  *        shift registers that expand GP outputs and drive the LEDs
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-08-31
  * @version 1.1
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
#include "main.h"
#include "hardware_drivers/74hc595.h"


// BSRR register: reset[15:0]set[15:0]. A 1 in the lower 16 bits will set the
// corresponding pin to high. A 1 in the upper 16 bits will set the corresponding
// pin to low. The CubeMX *_Pin labels are GPIO_PIN_x bitmasks, so they drop straight in.
#define PIN_SET(pin)  ((uint32_t)(pin))
#define PIN_RST(pin)  ((uint32_t)(pin) << 16)

static uint32_t half_cyc; // 177 cycles at 170MHz/480kHz

// Push-pull output at the fastest slew; CubeMX already enabled the port clock and set the pin low
static void pin_out_fast (GPIO_TypeDef *port, uint32_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &g);
}


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
void shift_bit(uint32_t bit)
{
    SHIFTREG_Clock_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Clock_Pin);
    SHIFTREG_Data_GPIO_Port->BSRR = bit ? PIN_SET(SHIFTREG_Data_Pin) : PIN_RST(SHIFTREG_Data_Pin);
        wait_half();
        SHIFTREG_Clock_GPIO_Port->BSRR = PIN_SET(SHIFTREG_Clock_Pin);
        wait_half();
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
    { shift_bit((byte >> i) & 1); }
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
        LED_Latch_GPIO_Port->BSRR = PIN_SET(LED_Latch_Pin);
        __NOP(); __NOP(); // very short delay lol
        LED_Latch_GPIO_Port->BSRR = PIN_RST(LED_Latch_Pin);
    } else {
        SHIFTREG_Clock_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Clock_Pin);
        wait_half();
        SHIFTREG_Latch_GPIO_Port->BSRR = PIN_SET(SHIFTREG_Latch_Pin);
        wait_half();
        SHIFTREG_Latch_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Latch_Pin);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_shiftreg_init : Set up the LED latch pin (data and clock go out over SPI)
 ----------------------------------------------------------------------------------
*/
void led_shiftreg_init(void)
{

    half_cyc = ((uint64_t)SystemCoreClock / ((uint64_t)2UL * (uint64_t)SHIFT_REG_SERIAL_HZ));

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Debug exception & monitor ctrl register setup, trace enable
    DWT->CYCCNT = 0; // cycle counter for PWM speed
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // enable cycle counter. PWM period based on number of CPU clock cycles

    pin_out_fast(LED_Latch_GPIO_Port, LED_Latch_Pin); // latch pin
    LED_Latch_GPIO_Port->BSRR = PIN_RST(LED_Latch_Pin);

}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC shiftreg_init : Set up pins for the shift registers for MCP4728 address programming (serial, clock, latch)
 ----------------------------------------------------------------------------------
*/
void shiftreg_init(void)
{

    half_cyc = ((uint64_t)SystemCoreClock / ((uint64_t)2UL * (uint64_t)SHIFT_REG_SERIAL_HZ));

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Debug exception & monitor ctrl register setup, trace enable
    DWT->CYCCNT = 0; // cycle counter for PWM speed
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // enable cycle counter. PWM period based on number of CPU clock cycle

    pin_out_fast(SHIFTREG_Data_GPIO_Port, SHIFTREG_Data_Pin); // initialize the pins needed - serial data
    pin_out_fast(SHIFTREG_Clock_GPIO_Port, SHIFTREG_Clock_Pin); // serial clock
    pin_out_fast(SHIFTREG_Latch_GPIO_Port, SHIFTREG_Latch_Pin); // latch pin
    SHIFTREG_Data_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Data_Pin);
    SHIFTREG_Clock_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Clock_Pin);
    SHIFTREG_Latch_GPIO_Port->BSRR = PIN_RST(SHIFTREG_Latch_Pin);

}