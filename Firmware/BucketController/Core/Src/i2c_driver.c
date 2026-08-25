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

#include "i2c_driver.h"
#include "stm32g4xx.h"
#include "stm32g4xx_ll_i2c.h"
#include <stdbool.h>
#include "mcp_funcs.h"
#include <stdio.h>

#define TRANSFER_US(n) (((((n) + 1) * 9 + 3) * 5) / 2) // transfer time in microseconds for any number of bytes
#define BUDGET_LIMIT ((I2C_SLOT_PERIOD_US * 92) / 100) // max time budget during send frame.


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

    slot_t frame[I2C_SLOTS_PER_SUPERFRAME];
    uint8_t scan_len; // leading descriptors that are scan reads: when to fire the callback

    volatile uint8_t slot; // ISR index into frame
    volatile uint8_t idx; // descriptor within the current slot
    volatile bool busy; // set at slot start, cleared when last descriptor completes

    i2c_stats_t st; // diagnostics
    uint32_t    t0;
} bus_t;


static bus_t B[I2C_NUM_BUSES];
//static i2c_scan_cb scan_cb;
static uint32_t cyc_us; // cpu cycles per microsec
static volatile bool  running;


/*=============================== LOW LEVEL HELPERS ================================*/

/**
 ----------------------------------------------------------------------------------
  INTERNAL wait_flag : bus, flag, abort on negative acknoledge?, microseconds -> bool
  Polls the I2C status register until transfer suceeds or fails 
 ----------------------------------------------------------------------------------
*/
static bool wait_flag(bus_t *bus_ptr, uint32_t flag, bool abort_on_nack, uint32_t us)
{
    uint32_t t0 = DWT->CYCCNT, lim = us * cyc_us;
    for (;;) {
        uint32_t isr = bus_ptr->i2c->ISR;
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
static inline void clear_flags(bus_t *bus_ptr)
{
    bus_ptr->i2c->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF |
                    I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
}


/**
 ----------------------------------------------------------------------------------
  INTERNAL bus_idle : bus, microseconds -> bool
  Clears stale flags and confirms the bus is actually free (idle) before starting
 ----------------------------------------------------------------------------------
*/
static bool bus_idle(bus_t *bus_ptr, uint32_t us)
{
    clear_flags(bus_ptr);
    uint32_t t0 = DWT->CYCCNT, lim = us * cyc_us;
    while (bus_ptr->i2c->ISR & I2C_ISR_BUSY)
        if ((DWT->CYCCNT - t0) > lim) return false;
    return true;
}

/**
 ----------------------------------------------------------------------------------
  INTERNAL abort_transfer : bus -> void
  Aborts transfer
 ----------------------------------------------------------------------------------
*/
static void abort_transfer(bus_t *bus_ptr)
{
    bus_ptr->i2c->ISR = I2C_ISR_TXE; // flush TXDR
    clear_flags(bus_ptr);

    if (bus_ptr->i2c->ISR & I2C_ISR_BUSY) {
        bus_ptr->i2c->CR2 |= I2C_CR2_STOP;
        uint32_t t0 = DWT->CYCCNT;
        while ((bus_ptr->i2c->ISR & I2C_ISR_BUSY) &&
               (DWT->CYCCNT - t0) < 1000U * cyc_us) { }
        if (bus_ptr->i2c->ISR & I2C_ISR_BUSY) {
            LL_I2C_Disable(bus_ptr->i2c);
            LL_I2C_Enable(bus_ptr->i2c);
        }
    }
    clear_flags(bus_ptr);
}


/*=============================== PUBLIC I2C FUNCTIONS ================================*/
/* Higher-level functions to interact with I2C */


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_init : Initialize the i2c busses
 ----------------------------------------------------------------------------------
*/
void i2c_init(void)
{
    static I2C_TypeDef *const P[I2C_NUM_BUSES] = { I2C1, I2C2, I2C3, I2C4 };

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        DWT->CYCCNT = 0;
        DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    }
    cyc_us = SystemCoreClock / 1000000U;

    running = false;

    for (uint8_t bus = 0; bus < I2C_NUM_BUSES; bus++)
        B[bus].i2c = P[bus];
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_read : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Read (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_read (uint8_t bus, uint8_t addr, uint8_t *data, uint8_t nbytes)
{
    bus_t *b = &B[bus];
    if(running || nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data) || nbytes == 0) return false;
    if(!bus_idle(b, IDLE_TIMEOUT_US)) // idle timeout 1000us
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
        if (!wait_flag(b, I2C_ISR_RXNE, true, BLOCK_TIMEOUT_US))
        {
            abort_transfer(b);
            return false;
        }
        data[i] = (uint8_t)b->i2c->RXDR;
    }


    if(!wait_flag(b, I2C_ISR_STOPF, false, BLOCK_TIMEOUT_US))
    {
        abort_transfer(b);
        return false;
    }

    if(b->i2c->ISR & I2C_ISR_NACKF)
    {
        abort_transfer(b);
        return false;
    }

    clear_flags(b);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_write : I2C bus (0,1,2,3), chip address 
                       data pointer, number of bytes to write -> bool
  Write (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_write (uint8_t bus, uint8_t addr, const uint8_t *data, uint8_t nbytes)
{
    bus_t *b = &B[bus];
    if(running || nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data)) return false;
    if(!bus_idle(b, IDLE_TIMEOUT_US)) // idle timeout 1000us
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
        if (!wait_flag(b, I2C_ISR_TXIS, true, BLOCK_TIMEOUT_US))
        {
            abort_transfer(b);
            return false;
        }
        b->i2c->TXDR = data[i];
    }

    if(!wait_flag(b, I2C_ISR_STOPF, false, BLOCK_TIMEOUT_US))
    {
        abort_transfer(b);
        return false;
    }

    if(b->i2c->ISR & I2C_ISR_NACKF)
    {
        abort_transfer(b);
        return false;
    }

    clear_flags(b);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_probe : I2C bus (0,1,2,3), chip address -> bool
  Returns whether a specific I2C chip is free or not
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (uint8_t bus, uint8_t addr)
{
    uint8_t test_byte = 0xAA;
    return i2c_write(bus, addr, &test_byte, sizeof(test_byte));
}


/*=============================== DEBUG ================================*/


#include <stdio.h>
#include <stdint.h>
#define GIRLMODER MODER

volatile uint32_t dbg_cyc_us;
volatile uint32_t dbg_cr1;
volatile uint32_t dbg_isr_before;
volatile uint32_t dbg_isr_after;
volatile uint32_t dbg_i2c_addr;
volatile uint8_t  dbg_probe_result;
volatile uint8_t  ports[I2C_NUM_BUSES][MCP23017_PER_BUS][2];
volatile uint16_t init_fails;
volatile uint32_t read_errors;


/**
 ----------------------------------------------------------------------------------
  PUBLIC i2c_debug_msg : Writes messages to USART2 debug.
 ----------------------------------------------------------------------------------
*/
void i2c_debug_msg(void)
{
    static const uint32_t EXPECT[I2C_NUM_BUSES] = {
    0x40005400,   /* I2C1 */
    0x40005800,   /* I2C2 */
    0x40007800,   /* I2C3 */
    0x40008400,   /* I2C4 */
    };

    printf("\r\n\nDIAGNOSTICS: I2C start-up\r\n");

    for (uint8_t bus = 0; bus < I2C_NUM_BUSES; bus++)
    {
        printf("    B[%u].i2c    = %08lX (expect %08lX) %s\r\n",
               bus, (uint32_t)B[bus].i2c, EXPECT[bus],
               ((uint32_t)B[bus].i2c == EXPECT[bus]) ? "OK" : "MISMATCH");
    }

    printf("    cyc_us      = %lu (expect 170)\r\n", cyc_us);
    printf("    HSIRDY      = %lu (expect 1)\r\n", (RCC->CR >> 10) & 1);
    printf("    I2C1EN      = %lu (expect 1)\r\n", (RCC->APB1ENR1 >> 21) & 1);
    printf("    CR1         = %08lX ([0]=PE should be 1)\r\n", I2C1->CR1);
    printf("    TIMINGR     = %08lX (expect 00300617)\r\n", I2C1->TIMINGR);
    printf("    RCC->CCIPR  = %08lX (bits 13:12 = I2C1SEL, expect 10b)\r\n", RCC->CCIPR);
    printf("    MODER       = %08lX\r\n", GPIOB->GIRLMODER);
    printf("    AFRH        = %08lX (low 2 nibbles expect 4,4)\r\n", GPIOB->AFR[1]);
    printf("    ISR before  = %08lX ([15]=BUSY should be 0)\r\n", I2C1->ISR);

    bool r = i2c_probe(0, 0x20);
  
    printf("    probe       = %d\r\n", r);
    printf("    ISR after   = %08lX\r\n\n", I2C1->ISR);

    // probe sweep
    for (uint8_t bus = 0; bus < I2C_NUM_BUSES; bus++) {
        printf("  + Probing bus %u... Present: ", bus);
        for (uint16_t a = 0x08; a <= 0x77; a++)
        {
            if (i2c_probe(bus, (uint8_t)a)) printf(" 0x%02X", a);
        }
        printf("\r\n");
    }
    printf("\rCompleted I2C startup.\r\n\n");
}