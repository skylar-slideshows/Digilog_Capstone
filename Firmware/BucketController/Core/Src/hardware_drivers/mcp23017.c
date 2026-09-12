/**
  **********************************************************************************
  * MCP23017 GPIO EXPANDER DRIVER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file hardware_drivers/mcp23017.c
  * @brief 
  *
  * @author Skylar Denno (denno.o@northeastern.edu), Darya Petrova (petrov.da@northeastern.edu)
  *         
  * @date 2026-08-23
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

#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/i2c_driver.h"
#include <stdbool.h>
#include "stm32g474xx.h"

uint8_t mcp23017_value_cache[4]  // 4 I2C channels
                            [8]  // 8 possible addresses per channel
                            [2]; // 2 bytes cached per chip

/**
 ----------------------------------------------------------------------------------
  @brief PRIVATE mcp23017_init_read : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  SPECIFY REGISTER, NOT FOR THE SCHEDULER (DATA INTENSIVE)
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init_read (I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg, uint8_t *value)
{
    if (!i2c_write(bus, addr, &reg, 1)) return false; // set pointer
    return i2c_read(bus, addr, value, 1);             // read 1 byte
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus, chip's address (0x20, 0x21, 0x22), output -> bool
  Reads both GPIOA and B registers ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *out)
{
    return i2c_read(bus, addr, out, 2); //A, B
}

static uint8_t i2c_to_int (I2C_TypeDef *bus)
{
  if(bus == I2C1) return 0;
  if(bus == I2C2) return 1;
  if(bus == I2C3) return 2;
  if(bus == I2C4) return 3;
  return 4; // indicate failure
}

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_poll_to_cache: I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Reads both GPIOA and GOIPB registers in one transaction, and caches them for async
  reading later
 ----------------------------------------------------------------------------------
*/
bool mcp23017_poll_to_cache (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t *out_dest = mcp23017_value_cache[i2c_to_int(bus)][addr & 0x08];
    return mcp23017_read(bus, addr, out_dest);
}

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read_from_cache : I2C bus, chip's address (0x20, 0x21, 0x22) -> 16bit (GPIOB, GPIOA)
  Reads cached values on GPIOA and GOIPB without running an i2c transaction
 ----------------------------------------------------------------------------------
*/
void mcp23017_read_from_cache (I2C_TypeDef *bus, uint8_t addr, uint8_t *out) {
    uint8_t *from = mcp23017_value_cache[i2c_to_int(bus)][addr & 0x08];
    out[0] = from[0];
    out[1] = from[1];
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_write (I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_write(bus, addr, data, 2);
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with a no interrupt pin setup.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t iocon;
    if (!i2c_probe(bus, addr)) return false; // does chip exist?

    // write both ports seqop (0x0A and 0x0B), disabling auto-increment (see mcp_regs.h)
    {
        uint8_t io[3] = {MCP_IOCONA, IOCON_SEQOP, IOCON_SEQOP};
        if (!i2c_write(bus, addr, io, 3)) return false;
    }

    // write config data to one register at a time, returning false if fail
    if (!mcp23017_write(bus, addr, MCP_IODIRA, 0xFF)) return false; // pin direction: gpio inputs into stm32
    if (!mcp23017_write(bus, addr, MCP_IODIRB, 0xFF)) return false;
    if (!mcp23017_write(bus, addr, MCP_GPPUA, 0xFF)) return false; // use internal 100k pull up res
    if (!mcp23017_write(bus, addr, MCP_GPPUB, 0xFF)) return false;
    if (!mcp23017_write(bus, addr, MCP_IPOLA, 0x00)) return false; // no invert
    if (!mcp23017_write(bus, addr, MCP_IPOLB, 0x00)) return false;

    // check to make sure sequential mode (above) is off
    if (!mcp23017_init_read(bus, addr, MCP_IOCONA, &iocon)) return false;
    if (!(iocon & IOCON_SEQOP)) return false;

    // park the pointer (permanately since seq mode off) @ gpioA
    {
        uint8_t reg = MCP_GPIOA;
        return i2c_write(bus, addr, &reg, 1);
    }
}
