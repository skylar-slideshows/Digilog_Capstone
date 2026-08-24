/**
  **********************************************************************************
  * MCP23017 GPIO EXPANDER DRIVER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file mcp23017_driver.c
  * @brief 
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
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

#include "mcp_regs.h"
#include "mcp_funcs.h"
#include "i2c_driver.h"
#include <stdbool.h>


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Reads one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
static bool mcp23017_read(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *value)
{
    if(!i2c_write_b(bus, addr, &reg, 1)) return false; // set pointer
    return i2c_read_b(bus, addr, value, 1); // read 1 byte
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
static bool mcp23017_write(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value)
{
  uint8_t data[2] = { reg, value };
  return i2c_write_b(bus, addr, data, 2);
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus num, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with no interrupt pin setup.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init(uint8_t bus, uint8_t addr)
{
    uint8_t iocon;
    if(!i2c_probe(bus, addr)) return false; // does chip exist?

    // write both ports seqop (0x0A and 0x0B), disabling auto-increment (see mcp_regs.h)
    {
        uint8_t io[3] = { MCP_IOCONA, IOCON_SEQOP, IOCON_SEQOP };
        if(!i2c_write_b(bus, addr, io, 3)) return false;
    }

    // write config data to one register at a time, returning false if fail
    if(!mcp23017_write(bus, addr, MCP_IODIRA, 0xFF)) return false; // pin direction: gpio inputs into stm32
    if(!mcp23017_write(bus, addr, MCP_IODIRB, 0xFF)) return false;
    if(!mcp23017_write(bus, addr, MCP_GPPUA,  0xFF)) return false; // use internal 100k pull up res
    if(!mcp23017_write(bus, addr, MCP_GPPUB,  0xFF)) return false;
    if(!mcp23017_write(bus, addr, MCP_IPOLA,  0x00)) return false; // no invert
    if(!mcp23017_write(bus, addr, MCP_IPOLB,  0x00)) return false;

    // check to make sure sequential mode (above) is off
    if(!mcp23017_read(bus, addr, MCP_IOCONA, &iocon)) return false;
    if(iocon != IOCON_SEQOP) return false;

     // park the pointer (permanately since seq mode off) @ gpioA
    {
        uint8_t reg = MCP_GPIOA;
        return i2c_write_b(bus, addr, &reg, 1);
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init_all : void -> bool
  Initialize all MCP23017s described in macro defns.
 ----------------------------------------------------------------------------------
*/
static uint16_t mcp23017_fails;
uint16_t mcp23017_fail_query(void) { return mcp23017_fails; }

bool mcp23017_init_all(void)
{
    static const uint8_t ADDR[MCP23017_PER_BUS] = { 0x20, 0x21, 0x22 }; // address of each chip
    bool success = true;

    for(uint8_t bus = 0; bus < I2C_NUM_BUSES; bus++)
    {
      for(uint8_t chip = 0; chip < MCP23017_PER_BUS; chip++)
      {
        if(!mcp23017_init(bus, ADDR[chip]))
          {
            mcp23017_fails |= (1u << (bus * MCP23017_PER_BUS + chip));
            success = false;
          }
      }
    }
    return success;
}