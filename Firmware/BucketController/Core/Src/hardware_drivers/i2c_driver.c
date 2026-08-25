/**
  **********************************************************************************
  * I2C SCHEDULER AND DRIVER - DIGILOG CONSOLE
  **********************************************************************************
  * @file i2c_driver.c
  * @brief Sets up and manages traffic on the four I2C busses. Polls the MCP23017
  *        GPIO expanders at a fixed rate and provides the CV streams
  *        to MCP4728 DACs at a fixed rate.
  *
  * The bucket controller has four identical I2C busses, one for each channel it controls.
  * Each of these I2C busses contains 3 MCP23017s (0x20, 0x21, 0x22) and 5 MCP4728s
  * (0x60, 0x61, 0x62, 0x63, 0x64). The first two MCP23017s are connected to the rotation pins
  * of the rotary encoders on their respective channel are polled at a higher rate (1,500 times/sec)
  * than the third one (only 100 times/sec, every superframe), which is only connected to push buttons.
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

#include "hardware_drivers/i2c_driver.h"
#include "stm32g474xx.h"
#include "stm32g4xx_ll_i2c.h"
#include "system_stm32g4xx.h"
#include <stdbool.h>
#include "hardware_drivers/i2c_driver.h"

#define TRANSFER_US(n) (((((n) + 1) * 9 + 3) * 5) / 2) // transfer time in microseconds for any number of bytes
#define BUDGET_LIMIT ((I2C_SLOT_PERIOD_US * 92) / 100) // max time budget during send frame.
#define TIMEOUT_CYCLES ((uint64_t)TIMEOUT_US * (uint64_t)SystemCoreClock) / 1000000U


/*=============================== LOW LEVEL HELPERS ================================*/

/**
 ----------------------------------------------------------------------------------
  INTERNAL wait_flag : bus, flag, abort on negative acknoledge?, microseconds -> bool
  Polls the I2C status register until transfer suceeds or fails 
 ----------------------------------------------------------------------------------
*/
static bool wait_flag(I2C_TypeDef *bus, uint32_t flag, bool abort_on_nack)
{
    uint32_t time0 = DWT->CYCCNT;
    for (;;) {
        uint32_t isr = bus->ISR;
        if (isr & flag)                             return true;
        if (abort_on_nack && (isr & I2C_ISR_NACKF)) return false; // flag will never reach with NACK
        if (isr & (I2C_ISR_BERR | I2C_ISR_ARLO))    return false;
        if ((DWT->CYCCNT - time0) > TIMEOUT_CYCLES) return false;
    }
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL clear_flags : bus -> Void
  Clears flags on a bus
 ----------------------------------------------------------------------------------
*/
static inline void clear_flags(I2C_TypeDef *bus)
{
    bus->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF |
                    I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL bus_idle : bus, microseconds -> bool
  Clears stale flags and confirms the bus is actually free (idle) before starting
 ----------------------------------------------------------------------------------
*/
static bool bus_idle(I2C_TypeDef *bus)
{
    clear_flags(bus);
    uint32_t t0 = DWT->CYCCNT;
    while (bus->ISR & I2C_ISR_BUSY)
        if ((DWT->CYCCNT - t0) > TIMEOUT_CYCLES) return false;
    return true;
}

/**
 ----------------------------------------------------------------------------------
  INTERNAL abort_transfer : bus -> void
  Aborts transfer
 ----------------------------------------------------------------------------------
*/
static void abort_transfer(I2C_TypeDef *bus)
{
    bus->ISR = I2C_ISR_TXE; // flush TXDR
    clear_flags(bus);

    if (bus->ISR & I2C_ISR_BUSY) {
        bus->CR2 |= I2C_CR2_STOP;
        uint32_t t0 = DWT->CYCCNT;
        while ((bus->ISR & I2C_ISR_BUSY) &&
               (DWT->CYCCNT - t0) < TIMEOUT_CYCLES) { }
        if (bus->ISR & I2C_ISR_BUSY) {
            LL_I2C_Disable(bus);
            LL_I2C_Enable(bus);
        }
    }
    clear_flags(bus);
}


/*=============================== PUBLIC I2C FUNCTIONS ================================*/
/* Higher-level functions to interact with I2C */


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_init : Initialize the i2c busses, call on I2C1 ... I2C4
 ----------------------------------------------------------------------------------
*/
void i2c_debug_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        DWT->CYCCNT = 0;
        DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    }
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_read : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Read (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *data, uint8_t nbytes)
{
    if(nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data) || nbytes == 0) return false;
    if(!bus_idle(bus)) // idle timeout 1000us
    {
        abort_transfer(bus);
        return false;
    } // bus is idle

    // head of the read block
    LL_I2C_HandleTransfer(bus,
                          (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
                          LL_I2C_ADDRSLAVE_7BIT,
                          nbytes,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);

    // read the content and save at *data
    for(uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(bus, I2C_ISR_RXNE, true))
        {
            abort_transfer(bus);
            return false;
        }
        data[i] = (uint8_t)bus->RXDR;
    }


    if(!wait_flag(bus, I2C_ISR_STOPF, false))
    {
        abort_transfer(bus);
        return false;
    }

    if(bus->ISR & I2C_ISR_NACKF)
    {
        abort_transfer(bus);
        return false;
    }

    clear_flags(bus);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_write : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Write (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_write (I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint8_t nbytes)
{
    if(nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data)) return false;
    if(!bus_idle(bus)) // idle timeout 1000us
    {
        abort_transfer(bus);
        return false;
    } // bus is idle

    // head of the message
    LL_I2C_HandleTransfer(bus,
                          (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
                          LL_I2C_ADDRSLAVE_7BIT,
                          nbytes,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    // send the content of the message
    for(uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(bus, I2C_ISR_TXIS, true))
        {
            abort_transfer(bus);
            return false;
        }
        bus->TXDR = data[i];
    }

    if(!wait_flag(bus, I2C_ISR_STOPF, false))
    {
        abort_transfer(bus);
        return false;
    }

    if(bus->ISR & I2C_ISR_NACKF)
    {
        abort_transfer(bus);
        return false;
    }

    clear_flags(bus);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_probe : I2C bus (0,1,2,3), chip address -> bool
  Returns whether a specific I2C chip is free or not
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t test_byte = 0xAA;
    return i2c_write(bus, addr, &test_byte, sizeof(test_byte));
}