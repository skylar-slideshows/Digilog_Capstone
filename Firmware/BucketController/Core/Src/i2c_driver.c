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

#include "i2c_driver.h"
#include "stm32g474xx.h"
#include "stm32g4xx.h"
#include "stm32g4xx.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_dmamux.h"
#include "stm32g4xx_ll_tim.h"
#include <string.h>
#include <stdbool.h>

#define I2C_TIMINGR_400K 0x1032050AU // i2c uses the HSI 16mhz clock. we need to verify signal looks good on scope
#define TRANSFER_US(n) (((((n) + 1) * 9 + 3) * 5) / 2) // transfer time in microseconds for any number of bytes
#define RESTART_US 4 // 4 microsec restart time
#define BUDGET_LIMIT ((I2C_SLOT_PERIOD_US * 92) / 100) // max time budget during send frame.
#define I2C_MAX_TRANSFER_SLOT 5 // max num descriptors on one slot (slot_t.x[4] max)
#define I2C_NUM_BUSES 4 // four i2c busses
#define I2C_SLOTS_PER_FRAME 15 // superframe length (how many I2C frames per superframe, at 1.5khz = 10ms)
#define I2C_MAX_TRANSFER_LEN 8 // as long as nbytes is <255 for each transfer, it can be done at once and does not need reload mode


/*=============================== DATA DEFINITIONS ================================*/

/**
 ----------------------------------------------------------------------------------
  INTERNAL slot type : number and microseconds time budget during the send frame
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    uint8_t n;
    uint16_t budget_us;
    const i2c_transfer_t *x[I2C_MAX_TRANSFER_SLOT];
} slot_t;


/**
 ----------------------------------------------------------------------------------
  INTERNAL bus type : hardware identity params, set at init
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    I2C_TypeDef *i2c; // I2C 1, 2, 3, 4?
    DMA_Channel_TypeDef *dtx, *drx; // direct pointers (direct transfer and direct recieve)
    DMA_TypeDef *dma; // DMA1 all four busses
    uint32_t ifcr_tx; // pre-shifted 0F mask for this channel's flags. Computed once to optimize.
    uint32_t ifcr_rx;
    GPIO_TypeDef *port; // needed for bus recovery after send
    uint32_t pin_scl, pin_sda;
    uint32_t af; // to restore flag after send

    slot_t frame[I2C_SLOTS_PER_FRAME]; // set at init
    uint8_t scan_len; // leading descriptors that are scan reads: when to fire the callback

    volatile uint8_t slot; // ISR index into frame
    volatile uint8_t idx; // descriptor within the current slot
    volatile bool busy; // set at slot start, cleared when last descriptor completes

    i2c_stats_t st; // diagnostics
    uint32_t    t0;
} bus_t;


static bus_t B[I2C_NUM_BUSES];
static i2c_scan_cb scan_cb;
static uint32_t cyc_us; // cpu cycles per microsec
static volatile bool  running;


/*=============================== FORWARD DECLARATIONS ================================*/
/* All from the STM32 I2C boilerplate */

static void dwt_init(void);
static void gpio_af(bus_t *bus);
static void periph_init(bus_t *bus);
static void bus_recover(bus_t *bus);
 
static bool wait_flag(bus_t *bus, uint32_t flag, bool abort_on_nack, uint32_t us);
static void clear_flags(bus_t *bus);
static bool bus_idle(bus_t *bus, uint32_t us);
static void abort_xfer(bus_t *bus);
 
static void launch(bus_t *bus, const i2c_transfer_t *x);
static void advance(bus_t *bus);


/*=============================== LOW LEVEL HELPERS ================================*/

/**
 ----------------------------------------------------------------------------------
  INTERNAL wait_flag : bus, flag, abort on negative acknoledge?, microseconds -> bool
  Polls the I2C status register until transfer suceeds or fails 
 ----------------------------------------------------------------------------------
*/
static bool wait_flag(bus_t *bus, uint32_t flag, bool abort_on_nack, uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT, lim = us * cyc_us;
    for (;;) {
        uint32_t isr = bus->i2c->ISR;
        if (isr & flag)                             return true;
        if (abort_on_nack && (isr & I2C_ISR_NACKF)) return false; // flag will never reach with NACK
        if (isr & (I2C_ISR_BERR | I2C_ISR_ARLO))    return false;
        if ((DWT->CYCCNT - t0) > lim)               return false;
    }
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL clear_flags : bus -> Void
  Clears flags on a bus
 ----------------------------------------------------------------------------------
*/
static inline void clear_flags(bus_t *bus)
{
    bus->i2c->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF |
                    I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL bus_idle : bus, microseconds -> bool
  Clears stale flags and confirms the bus is actually free (idle) before starting
 ----------------------------------------------------------------------------------
*/
static bool bus_idle(bus_t *bus, uint32_t us)
{
    clear_flags(bus);
    uint32_t t0 = DWT->CYCCNT, lim = us * cyc_us;
    while (bus->i2c->ISR & I2C_ISR_BUSY)
        if ((DWT->CYCCNT - t0) > lim) return false;
    return true;
}

/**
 ----------------------------------------------------------------------------------
  INTERNAL abort_xfer : I2C bus num, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register during initialization startup. ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
static void abort_transfer(bus_t *bus)
{
    bus->i2c->ISR = I2C_ISR_TXE; // flush TXDR
    clear_flags(bus);

    if (bus->i2c->ISR & I2C_ISR_BUSY) {
        bus->i2c->CR2 |= I2C_CR2_STOP;
        uint32_t t0 = DWT->CYCCNT;
        while ((bus->i2c->ISR & I2C_ISR_BUSY) &&
               (DWT->CYCCNT - t0) < 1000u * cyc_us) { }
        if (bus->i2c->ISR & I2C_ISR_BUSY) {
            LL_I2C_Disable(bus->i2c);
            LL_I2C_Enable(bus->i2c);
        }
    }
    clear_flags(bus);
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL dwt_init : Initialize data watchpoint and trace for debugging
 ----------------------------------------------------------------------------------
*/
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    cyc_us = SystemCoreClock / 1000000U;
}


/*=============================== PUBLIC I2C FUNCTIONS ================================*/
/* Higher-level functions to interact with I2C */

/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_probe : I2C bus (0,1,2,3), chip address -> bool
  Returns whether a specific I2C chip is free or not
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (uint8_t bus, uint8_t addr)
{
    bus_t *b = &B[bus]; // i2c bus pointer

    if(running) return false; // busy
    if(!bus_idle(b, 1000)) // idle timeout 1000us
    {
        abort_transfer(b);
        return false;
    } // bus is idle

    LL_I2C_HandleTransfer(b->i2c,
                          (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
                          LL_I2C_ADDRSLAVE_7BIT,
                          0,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE); // ping

    if(!wait_flag(b, I2C_ISR_STOPF, false, 2000)) // block timeout 2000us
    {
        abort_transfer(b);
        return false;
    } // bus is good to use!

    bool ack = !(b->i2c->ISR & I2C_ISR_NACKF); // read before clearing
    clear_flags(b);
    return ack;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_read_b : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Read (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_read_b (uint8_t bus, uint8_t addr, uint8_t *data, uint8_t nbytes)
{
    bus_t *b = &B[bus];
    if(running || nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data)) return false;
    if(!bus_idle(b, 1000)) // idle timeout 1000us
    {
        abort_transfer(b);
        return false;
    } // bus is idle

    // head of the read block
    LL_I2C_HandleTransfer(b->i2c,
                          (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
                          LL_I2C_ADDRSLAVE_7BIT,
                          nbytes,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);

    // read the content and save at *data
    for(uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(b, I2C_ISR_RXNE, true, 2000))
        {
            abort_xfer(b);
            return false;
        }
        data[i] = (uint8_t)b->i2c->RXDR;
    }


    if(!wait_flag(b, I2C_ISR_STOPF, false, 2000))
    {
        abort_xfer(b);
        return false;
    }

    if(b->i2c->ISR & I2C_ISR_NACKF)
    {
        abort_xfer(b);
        return false;
    }

    clear_flags(b);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_write_b : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Write (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_write_b (uint8_t bus, uint8_t addr, const uint8_t *data, uint8_t nbytes)
{
    bus_t *b = &B[bus];
    if(running || nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data)) return false;
    if(!bus_idle(b, 1000)) // idle timeout 1000us
    {
        abort_transfer(b);
        return false;
    } // bus is idle

    // head of the message
    LL_I2C_HandleTransfer(b->i2c,
                          (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
                          LL_I2C_ADDRSLAVE_7BIT,
                          nbytes,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    // send the content of the message
    for(uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(b, I2C_ISR_TXIS, true, 2000))
        {
            abort_xfer(b);
            return false;
        }
        b->i2c->TXDR = data[i];
    }

    if(!wait_flag(b, I2C_ISR_STOPF, false, 2000))
    {
        abort_xfer(b);
        return false;
    }

    if(b->i2c->ISR & I2C_ISR_NACKF)
    {
        abort_xfer(b);
        return false;
    }

    clear_flags(b);
    return true;
}

