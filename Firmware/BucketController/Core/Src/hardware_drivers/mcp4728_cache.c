/**
  **********************************************************************************
  * MCP4728 Cache
  **********************************************************************************
  * @file hardware_drivers/mcp4728_cache.c
  * @brief 
  *
  * @author Darya Petrova (petrov.da@northeastern.edu)
  *         
  * @date 2026-09-15
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

#include <stdint.h>

#include "hardware_drivers/mcp4728.h"
#include "stm32g474xx.h"

static inline uint8_t i2c_to_int (I2C_TypeDef *bus)
{
    if (bus == I2C1) return 0;
    if (bus == I2C2) return 1;
    if (bus == I2C3) return 2;
    if (bus == I2C4) return 3;
    return 4; // indicate failure
}

mcp4728_output_value_t mcp4728_value_cache[4]  // 4 I2C channels
                                          [8]  // 8 possible addresses per channel
                                          [4]; // 4 12-bit outputs cached per chip

/*
 * Writes an output value to the mcp4728 cache
 */
void mcp4728_cache_write (
    I2C_TypeDef *bus,          // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,              // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    uint8_t output_channel,    // Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t val // 12-bit output value
)
{
    mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07][output_channel] = val;
}

/*
 * Performs a fast write operation on the MCP4728 using cached output values
 *
 * This function performs a fast write operation on the MCP4728 device, updating
 * the DAC output values for all four channels in a single I2C transaction. Gain,
 * voltage reference, power mode options are not configurable. They are not changed
 * in the transaction. Prior values are used.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_cache_flush_fastWrite (
    I2C_TypeDef *bus, // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr      // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
)
{
    mcp4728_output_value_t *outs = mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07];
    return mcp4728_fastWrite(bus, addr, outs[0], outs[1], outs[2], outs[3]);
}
