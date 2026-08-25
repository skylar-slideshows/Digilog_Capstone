/**
  **********************************************************************************
  * MCP23017 REGISTER CONSTRAINTS AND FUNCTIONS HEADER - DIGILOG CONSOLE
  **********************************************************************************
  * @file mcp_regs.h
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

#ifndef MCP23017_DRIVER_H
#define MCP23017_DRIVER_H
#include <stdint.h>
#include <stdbool.h>
#include "stm32g474xx.h"


/*=============================== REGISTER DEFINITIONS ================================*/
/** @brief Addresses below are for IOCON.BANK = 0 (POR default), where the A and B
   registers of each pair are interleaved. We never change BANK to keep fixed addresses. */

#define MCP_IODIRA 0x00 // direction, port A: 1 = input, POR 0xFF, we will write 0xFF (all inputs)
#define MCP_IODIRB 0x01 // same as above, B
#define MCP_IPOLA  0x02 // input polarity, port A. 1 = invert the value read from GPIO. we use 0
#define MCP_IPOLB  0x03 // same as above, B
#define MCP_IOCONA 0x0A // config; written FIRST, while still sequential, so it can be one 3-byte write
#define MCP_IOCONB 0x0B // same as above, B
#define MCP_GPPUA  0x0C // pull-up enable, port A: 1 = internal ~100k on, we will write 0xFF to use internal
#define MCP_GPPUB  0x0D // same as above, B
#define MCP_GPIOA  0x12 // read = live pin state. WRITE lands in OLAT, not here, pointer is parked here
#define MCP_GPIOB  0x13 // never addressed directly at runtime: the A/B toggle reaches it from 0x12
#define IOCON_BANK 0x80 // input output command (IOCON) - do not write this bit!

/** @brief 0 on reset - address pointer increments after each byte
1 = byte mode (what we want) no increment so we can access the same address repeatedly
 w/o sending control bytes (reduces traffic on the bus) */
#define IOCON_SEQOP  0x20

#define MCP_POR_IODIR 0xFF // keep power-on reset states
#define MCP_POR_OTHER 0x00


/*=============================== FUNCTIONS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Reads one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_read(I2C_TypeDef *bus, uint8_t addr, uint8_t reg, uint8_t *value);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_write(I2C_TypeDef *bus, uint8_t addr, uint8_t reg, uint8_t value);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus num, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with no interrupt pin setup.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init(I2C_TypeDef *bus, uint8_t addr);

#endif