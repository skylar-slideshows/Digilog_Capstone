/**
  **********************************************************************************
  * MCP23017 / MCP4728 DRIVER FUNCTIONS HEADER - DIGILOG CONSOLE
  **********************************************************************************
  * @file mcp_funcs.h
  * @brief All public funtions for using the MCP23017s and MCP4728s
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
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

#ifndef MCP_FUNCS
#define MCP_FUNCS

#include <stdint.h>
#include <stdbool.h>


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Reads one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
static bool mcp23017_read(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *value);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
static bool mcp23017_write(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t value);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus num, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with no interrupt pin setup.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init(uint8_t bus, uint8_t addr);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init_all : void -> bool
  Initialize all MCP23017s described in macro defns.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init_all(void);

#endif