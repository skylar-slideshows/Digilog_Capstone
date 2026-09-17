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
#include <stdatomic.h>

#include "hardware_drivers/mcp4728_cache.h"
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

typedef atomic_uint_fast64_t mcp4728_atomic_channel_output_value_t;    // 64 bit value represents 4 DAC channels
volatile mcp4728_atomic_channel_output_value_t mcp4728_value_cache[4]  // 4 I2C channels
                                                                  [8]; // 8 possible addresses per channel
typedef union
{
    uint64_t whole;
    uint16_t word[4];
} memory_abusive_64bit_word;

/*
 * Writes an output value to the mcp4728 cache
 */
void mcp4728_cache_write_single (
    I2C_TypeDef *bus,          // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,              // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    uint8_t output_channel,    // Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t val // 12-bit output value
)
{
    memory_abusive_64bit_word newval;
    newval.whole = atomic_load_explicit(&(mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07]), memory_order_relaxed);

    newval.word[output_channel] = val;

    atomic_store_explicit(&(mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07]), newval.whole, memory_order_relaxed);
}

/*
 * Writes an output value to the mcp4728 cache
 */
void mcp4728_cache_write_multi (
    I2C_TypeDef *bus,             // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                 // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    uint8_t *output_channels,     // Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t *vals, // 12-bit output value
    uint8_t output_channel_count  // length of lists
)
{
    memory_abusive_64bit_word newval;
    newval.whole = atomic_load_explicit(&(mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07]), memory_order_relaxed);

    for(uint8_t i = 0; i < output_channel_count; i++){
        newval.word[output_channels[i]] = vals[i];
    }

    atomic_store_explicit(&(mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07]), newval.whole, memory_order_relaxed);
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
    memory_abusive_64bit_word cached;
    cached.whole = atomic_load_explicit(&(mcp4728_value_cache[i2c_to_int(bus)][addr & 0x07]), memory_order_relaxed);

    return mcp4728_fastWrite(bus, addr, cached.word[0], cached.word[1], cached.word[2], cached.word[3]);
}
