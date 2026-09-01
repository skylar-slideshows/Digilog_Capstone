/**
  **********************************************************************************
  * SHIFT REGISTER DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file 74hc595.h
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
#include <stdbool.h>

#ifndef HC595_H
#define HC595_H


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC shift_bit : uint32 -> Shifts a single bit out.
 ----------------------------------------------------------------------------------
*/
void shift_bit(uint32_t bit, bool led);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC shift_byte : Shift out a whole byte to the DAC LDAC shift registers
  (DAC ONLY NO LED). Goes LSB to MSB
 ----------------------------------------------------------------------------------
*/
void shift_byte(uint8_t byte);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC latch_out : Pulse latch low to update register outputs
 ----------------------------------------------------------------------------------
*/
void latch_out(bool led);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_shiftreg_init : Set up pins for the shift registers for leds (serial, clock, latch)
 ----------------------------------------------------------------------------------
*/
void led_shiftreg_init(void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC dac_shiftreg_init : Set up pins for the shift registers for MCP4728 address programming (serial, clock, latch)
 ----------------------------------------------------------------------------------
*/
void dac_shiftreg_init(void);

#endif