/**
  **********************************************************************************
  * MCP4728 REGISTER CONSTRAINTS AND FUNCTIONS HEADER - DIGILOG CONSOLE
  **********************************************************************************
  * @file mcp_4728.h
  * @brief MCP23017s and MCP4728s are scanned at constant rate on I2C, and no interrupt
  *        pins are used, for simplicity. Only the necessary parts for this case are included.
  *
  * Transcribed from MCP23017/MCP23S17 DS20001952C and MCP4728 DS22187E.
  * This file is the transcription boundary: every number below was copied
  * from a PDF by hand. Re-check it against the datasheet revision for the
  * silicon actually being purchased before a production run.
  *
  * @author Skylar Denno (denno.o@northeastern.edu), Darya Petrova (petrov.da@northeastern.edu)
  * @date 2026-08-22
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

#ifndef MCP4728_DRIVER_H
#define MCP4728_DRIVER_H
#include <stdint.h>

/*=============================== REGISTER DEFINITIONS ================================*/
/** @brief MCP4728 has no register map. The first byte after the address byte is an opcode.
Only intialization commands listed below. During operation, no commands at all. Runs statically */

#define DAC_CMD_WRITE_VREF 0x80 // 0b1000 for each output
#define DAC_CMD_WRITE_PD   0xA0 // 0b1010 pdA pdB, then pdC pdD 0000
#define DAC_CMD_WRITE_GAIN 0xC0 // 0b1100 GA GB GC GD

#define DAC_VREF_VDD      0
#define DAC_VREF_INTERNAL 1 // 2.048V -> using this one, it is ratiometric with VDD
#define DAC_GAIN_X1       0
#define DAC_GAIN_X2       1 // 4.096V

#define DAC_PD_NORMAL 0
#define DAC_PD_1K     1 // 1kR to gnd

#define DAC_GC_ADDR      0x00 // general call, used to verify initialization was written at start
#define DAC_GC_READ_ADDR 0x0C

#define MCP23017_BASE 0x20 // addrs set in hardware by pins A[2:0]
#define MCP4728_BASE  0x60 // addrs set during startup by the dedicated 74HC595 programmer lines


/*=============================== FUNCTIONS ================================*/

#endif