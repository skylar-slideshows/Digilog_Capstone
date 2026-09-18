/**
  **********************************************************************************
  * MCP4728 HIGH-LEVEL CACHE WRAPPER
  **********************************************************************************
  * @file mcp4728_cache.h
  * @brief Provides a higher-level cache wrapper around mcp4728 calls
  * A cache provides async access to DAC output values, and it can be synchronously
  * flushed as needed for I2C scheduling
  *
  * @author Darya Petrova (petrov.da@northeastern.edu)
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

#ifndef INC_MCP4728_CACHE_H_
#define INC_MCP4728_CACHE_H_

#include <stdint.h>

#include "hardware_drivers/mcp4728.h"
#include "stm32g474xx.h"

/**
 * @brief Writes an output value to the mcp4728 cache
 */
void mcp4728_cache_write (
    I2C_TypeDef *bus,          //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,              //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    uint8_t output_channel,    //!< Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t val //!< 12-bit output value
);

/**
 * @brief Performs a fast write operation on the MCP4728 using cached output values
 *
 * This function performs a fast write operation on the MCP4728 device, updating
 * the DAC output values for all four channels in a single I2C transaction. Gain,
 * voltage reference, power mode options are not configurable. They are not changed
 * in the transaction. Prior values are used.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_cache_flush_fastWrite (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr      //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
);

#endif
