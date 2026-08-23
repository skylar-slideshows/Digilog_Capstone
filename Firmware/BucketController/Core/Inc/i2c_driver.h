/**
  **********************************************************************************
  * I2C SCHEDULER AND DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file i2c_driver.h
  * @brief Polls the MCP23017 GPIO expanders at a fixed rate and provides the CV streams
  *        to MCP4728 DACs at a fixed rate.
  *
  * The bucket controller has four identical I2C busses, one for each channel it controls.
  * Each of these I2C busses contains 3 MCP23017s (0x20, 0x21, 0x22) and 5 MCP4728s
  * (0x60, 0x61, 0x62, 0x63, 0x64). The first two MCP23017s are connected to the rotation pins
  * of the rotary encoders on their respective channel are polled at a higher rate (1,500 times/sec)
  * than the third one (only 100 times/sec), which is only connected to push buttons.
  * The MCP4728s are fed a 12-bit, low speed 600Hz sample rate stream and provide 20 control
  * voltage DAC channels per console channel. I2C busses run at fast rate (400kHz).
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

/* PIN ASSIGNMENTS (for one I2C bus / 1 channel):

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

#ifdef I2C_DRIVER_H
#define I2C_DRIVER_H

#endif