/**
  **********************************************************************************
  * I2C SCHEDULER AND DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file i2c_driver.h
  * @brief Polls the MCP23017 GPIO expanders at a fixed rate and provides the CV streams
  *        to MCP4728 DACs at a fixed rate.
  *
  * The bucket controller has four identical I2C busses, one for each channel it controls.
  * I2C busses run at fast rate (400kHz) and loop a fixed cycle of polling the 23017s and sending to 4728s.
  * Each of these I2C busses contains 3 MCP23017s (0x20, 0x21, 0x22) and 5 MCP4728s
  * (0x60, 0x61, 0x62, 0x63, 0x64). The first two MCP23017s are connected to the rotation pins
  * of the rotary encoders on their respective channel and must be polled at a higher rate (1,500 times/sec)
  * than the third one (MCP23017 #3) (only 100 times/sec), which is only connected to push buttons.
  * The MCP4728s are fed a 12-bit, low speed 600Hz sample rate stream.
  * 
  * The fixed loop runs 1,500 times per second (667us = 1 "slot"):
  *  - read MCP23017 #1 0x20 (73us - cumulative 73us)
  *  - read MCP23017 #2 0x21 (73us - cumulative 146us)
  *  - fast write to one of the 5 DAC chips (210us - cumulative 356us)
  *  - fast write to one of the 5 DAC chips (210us - cumulative 566us)
  *  - idle (101us - cumulative 667us)
  *
  * This slot loop is inside a "superframe" consisting of 15 slots (10,000us = 10ms = 100Hz)
  *  - The last slot in superframe additionally reads the MCP23017 #3 0x22 (dropping idle time to 28us for that slot)
  *    therefore, MCP23017 #3 (buttons only and does need fast polling) is read 100 times per second instead of 1500.
  *  - Only two of the DAC chips are written to each slot: this is the sequence [slot]DAC,DAC
  *     [0]0,1;  [1]2,3;  [2]4,0;  [3]1,2;  [4]3,4;
  *     [5]0,1;  [6]2,3;  [7]4,0;  [8]1,2;  [9]3,4;
  *    [10]0,1; [11]2,3; [12]4,0; [13]1,2; [14]3,4;
  *    Therefore, each individual DAC is written to 6 times per superframe, evenly spread (1.667ms between writes for one chip)
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

/** @attention
    PIN ASSIGNMENTS (for one I2C bus / 1 channel):

      0x20 (MCP23017 #1):
        - GPA0/GPA1 Input gain encoder A/B
        - GPA2/GPA3 FX encoder 1A A/B
        - GPA4/GPA5 FX encoder 1B A/B
        - GPA6/GPB0 FX encoder 2A A/B
        - GPB1/GPB2 FX encoder 2B A/B
        - GPB3/GPB4 FX encoder 3A A/B
        - GPB5/GPB6 FX encoder 3B A/B
        - GPA7/GPB7 N.C.

      0x21 (MCP23017 #2):
        - GPA0/GPA1 FX encoder 4A A/B
        - GPA2/GPA3 FX encoder 4B A/B
        - GPA4/GPA5 Pan encoder A/B
        - GPA6/GPB0 Input gain encoder PB / FX encoder 1A PB
        - GPB1/GPB2 FX encoder 1B & 2A PB
        - GPB3/GPB4 FX encoder 2B & 3A PB
        - GPB5/GPB6 FX encoder 3B & 4A PB
        - GPA7/GPB7 N.C.

      0x22 (MCP23017 #3):
        - GPA0/GPA1 FX encoder 4B PB / Pan encoder PB
        - GPA2/GPA3 Btn0/Btn1
        - GPA4/GPA5 Btn2/Btn3
        - GPA6/GPB0 Btn4/Btn5
        - GPB1/GPB2 Btn6/Btn7
        - GPB3/GPB4 Btn8/Btn9
        - GPB5/GPB6 Btn10/Btn11
        - GPA7/GPB7 N.C.

        i2c_init(uint8_);

        mcp23017_init(uint8_t bus, uint8_t addr, );

      0x60 (MCP4728 #1):
        - DACA Input gain CV
        - DACB Send 1L gain CV
        - DACC Send 1R gain CV
        - DACD Send 2L gain CV

      0x61 (MCP4728 #2):
        - DACA Send 2R gain CV
        - DACB Send 3L gain CV
        - DACC Send 3R gain CV
        - DACD Send 4 (mono) gain CV

      0x62 (MCP4728 #3):
        - DACA EQ HF Freq
        - DACB EQ HF Gain
        - DACC EQ HMF Freq
        - DACD EQ HMF Gain

      0x63 (MCP4728 #4):
        - DACA EQ HMF Q
        - DACB EQ LMF Freq
        - DACC EQ LMF Gain
        - DACD EQ LMF Q

      0x64 (MCP4728 #5):
        - DACA EQ LF Freq
        - DACB EQ LF Gain
        - DACC Channel out gain L (fader/pan/compressor)
        - DACD Channel out gain R (fader/pan/compressor)
*/

#include <stdint.h>
#include <stdbool.h>
#include "stm32g474xx.h"

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

/**
 ----------------------------------------------------------------------------------
  @brief  i2c_read : I2C bus, chip address, data pointer, number of bytes to write -> bool
  Read bytes from i2c chip (blocking)
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017_0, mcp4728s are write only
  @param data pointer to read data to
  @param nbytes number of bytes to read in
 ----------------------------------------------------------------------------------
*/
bool i2c_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *data, uint8_t nbytes);


/**
 ----------------------------------------------------------------------------------
  @brief i2c_write : I2C bus, chip address, data pointer, number of bytes to write -> bool
  Write bytes to i2c chip (blocking)
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x60 = MCP4728_0, mcp23017s are read only
  @param data pointer to data to write
  @param nbytes number of bytes to write
 ----------------------------------------------------------------------------------
*/
bool i2c_write (I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint8_t nbytes);


/**
 ----------------------------------------------------------------------------------
  @brief i2c_write_callback : I2C bus, chip address, data pointer, number of bytes to write, callback -> bool
  Write bytes to i2c chip (blocking), running a callback function after each byte is written
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x60 = MCP4728_0, mcp23017s are read only
  @param data pointer to data to write
  @param nbytes number of bytes to write
  @param callback void function which gets called after each byte is written over i2c, accepting a byte index arg
 ----------------------------------------------------------------------------------
*/
bool i2c_write_callback (I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint8_t nbytes, void (*callback)(uint8_t));


/**
 ----------------------------------------------------------------------------------
  @brief i2c_probe : I2C bus, chip address -> bool
  Returns whether a specific I2C chip is free or not
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017_0, 0x60 = MCP4728_0
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (I2C_TypeDef *bus, uint8_t addr);


/**
 ----------------------------------------------------------------------------------
  @brief i2c_init : Initialize the i2c busses
 ----------------------------------------------------------------------------------
*/
void i2c_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief i2c_probeall : Developer mode only - probe all i2c busses with uart output
 ----------------------------------------------------------------------------------
*/
void i2c_probeall (void);

#endif
