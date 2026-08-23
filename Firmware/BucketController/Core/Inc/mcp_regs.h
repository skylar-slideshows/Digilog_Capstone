/**
  **********************************************************************************
  * MCP23017 / MCP4728 REGISTER MAPS, CONSTRAINTS, COMMANDS - DIGILOG CONSOLE
  **********************************************************************************
  * @file mcp_regs.h
  * @brief 
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
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

#ifndef MCP_REGS_H
#define MCP_REGS_H
#include <stdint.h>


/*=============================== MCP23017 ================================*/

#define MCP_IODIRA   0x00
#define MCP_IODIRB   0x01
#define MCP_IPOLA    0x02
#define MCP_IPOLB    0x03
#define MCP_GPINTENA 0x04
#define MCP_GPINTENB 0x05
#define MCP_DEFVALA  0x06
#define MCP_DEFVALB  0x07
#define MCP_INTCONA  0x08
#define MCP_INTCONB  0x09
#define MCP_IOCONA   0x0A
#define MCP_IOCONB   0x0B
#define MCP_GPPUA    0x0C
#define MCP_GPPUB    0x0D
#define MCP_INTFA    0x0E
#define MCP_INTFB    0x0F
#define MCP_INTCAPA  0x10
#define MCP_INTCAPB  0x11
#define MCP_GPIOA    0x12
#define MCP_GPIOB    0x13
#define MCP_OLATA    0x14
#define MCP_OLATB    0x15

// Input/Output Control (IOCON) control macros
#define IOCON_BANK   0x80
#define IOCON_MIRROR 0x40
#define IOCON_SEQOP  0x20
#define IOCON_DISSLW 0x10
#define IOCON_HAEN   0x08
#define IOCON_ODR    0x04
#define IOCON_INTPOL 0x02

#define MCP_POR_IODIR 0xFF
#define MCP_POR_OTHER 0x00


/*=============================== MCP4728 ================================*/

#define DAC_CMD_FAST        0x00
#define DAC_CMD_MULTIWRITE  0x40
#define DAC_CMD_SEQWRITE    0x50
#define DAC_CMD_SINGLEWRITE 0x58
#define DAC_CMD_ADDRBITS    0x60
#define DAC_CMD_WRITE_VREF  0x80
#define DAC_CMD_WRITE_PD    0xA0
#define DAC_CMD_WRITE_GAIN  0xC0

#define DAC_FAST_HI(v12) ((uint8_t)(((v12) >> 8) & 0x0F))
#define DAC_FAST_LO(v12) ((uint8_t)((v12) & 0xFF))

#define DAC_VREF_VDD        0
#define DAC_VREF_INTERNAL   1 // 2.048V
#define DAC_GAIN_X1         0
#define DAC_GAIN_X2         1 // 4.096V

#define DAC_PD_NORMAL       0
#define DAC_PD_1K           1 // 1kR to gnd
#define DAC_PD_100K         2
#define DAC_PD_500K         3

#define DAC_GC_ADDR         0x00
#define DAC_GC_RESET        0x06
#define DAC_GC_WAKEUP       0x09
#define DAC_GC_SWUPDATE     0x08
#define DAC_GC_READ_ADDR    0x0C

#define MCP23017_BASE   0x20
#define MCP4728_BASE    0x60

#endif
