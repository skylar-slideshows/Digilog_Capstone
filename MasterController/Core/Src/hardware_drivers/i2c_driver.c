/**
  **********************************************************************************
  * I2C DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file i2c_driver.c
  * @brief See i2c_driver.h. The bucket controller's driver (Skylar Denno), on one bus.
  **********************************************************************************
*/

#include "hardware_drivers/i2c_driver.h"
#include "stm32g474xx.h"
#include "stm32g4xx_ll_i2c.h"
#include "system_stm32g4xx.h"
#include <stdbool.h>
#include "CONFIG.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

extern I2C_HandleTypeDef CONTROLS_I2C_HANDLE; // from main.c

static uint32_t timeout_cycles;

volatile uint32_t i2c_fail_isr;  // ISR register when the last transfer failed
volatile uint8_t  i2c_fail_step; // 1 busy before start, 2 data, 3 waiting for STOP, 4 NACK at end

static inline void note_fail (I2C_TypeDef *bus, uint8_t step)
{
    i2c_fail_isr = bus->ISR;
    i2c_fail_step = step;
}


/*=============================== LOW LEVEL HELPERS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL wait_flag : bus, flag, abort on negative acknoledge?, microseconds -> bool
  Polls the I2C status register until transfer suceeds or fails 
 ----------------------------------------------------------------------------------
*/
static bool wait_flag (I2C_TypeDef *bus, uint32_t flag, bool abort_on_nack)
{
    const uint32_t time0 = DWT->CYCCNT;
    uint32_t spins = 0;

    for (;;)
    {
        const bool timed_out = (DWT->CYCCNT - time0) > timeout_cycles;
        const uint32_t isr = bus->ISR;

        if (isr & flag) return true;
        if (abort_on_nack && (isr & I2C_ISR_NACKF)) return false;
        if (isr & (I2C_ISR_BERR | I2C_ISR_ARLO)) return false;
        if (timed_out) return false;

        if ((++spins & 0xFF) == 0 && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        {
            taskYIELD();
        }
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL clear_flags : bus -> Void
  Clears flags on a bus
 ----------------------------------------------------------------------------------
*/
static inline void clear_flags (I2C_TypeDef *bus)
{
    bus->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL bus_idle : bus, microseconds -> bool
  Clears stale flags and confirms the bus is actually free (idle) before starting
 ----------------------------------------------------------------------------------
*/
static bool bus_idle (I2C_TypeDef *bus)
{
    clear_flags(bus);
    return !(bus->ISR & I2C_ISR_BUSY);
}

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL abort_transfer : bus -> void
  Aborts transfer
 ----------------------------------------------------------------------------------
*/
static void abort_transfer (I2C_TypeDef *bus)
{
    bus->ISR = I2C_ISR_TXE; // flush TXDR
    clear_flags(bus);

    if (bus->ISR & I2C_ISR_BUSY)
    {
        bus->CR2 |= I2C_CR2_STOP;
        // uint32_t t0 = DWT->CYCCNT;
        /*while ((bus->ISR & I2C_ISR_BUSY) && (DWT->CYCCNT - t0) < timeout_cycles)
        {
        }*/
        if (bus->ISR & I2C_ISR_BUSY)
        {
            LL_I2C_Disable(bus);
            (void)bus->CR1;
            LL_I2C_Enable(bus);
        }
    }
    clear_flags(bus);
}


/*=============================== PUBLIC I2C FUNCTIONS ================================*/
/* Higher-level functions to interact with I2C */


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC i2c_init : 400 kHz on I2C2, cycle counter on for the timeouts
 ----------------------------------------------------------------------------------
*/
void i2c_init (void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    timeout_cycles = (uint32_t)(((uint64_t)TIMEOUT_US * (uint64_t)SystemCoreClock) / 1000000U);

    // CubeMX sets I2C2 up at 100 kHz; reading both expanders every millisecond needs 400 kHz
    CONTROLS_I2C_HANDLE.Init.Timing = CONTROLS_I2C_TIMING;
    if (HAL_I2C_Init(&CONTROLS_I2C_HANDLE) != HAL_OK)
    {
        printf("\r\nI2C2 setup FAILED\n");
    }
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC i2c_read : I2C bus (I2C1 ... I2C4 = I2C_TypeDef), chip address 
                       data pointer, number of bytes to write -> bool
  Read (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *data, uint8_t nbytes)
{
    if (nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data) || nbytes == 0) return false;
    if (!bus_idle(bus)) // idle timeout 1000us
    {
        note_fail(bus, 1);
        abort_transfer(bus);
        return false;
    } // bus is idle

    // head of the read block
    LL_I2C_HandleTransfer(
        bus,
        (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
        LL_I2C_ADDRSLAVE_7BIT,
        nbytes,
        LL_I2C_MODE_AUTOEND,
        LL_I2C_GENERATE_START_READ
    );

    // read the content and save at *data
    for (uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(bus, I2C_ISR_RXNE, true))
        {
            note_fail(bus, 2);
            abort_transfer(bus);
            return false;
        }
        data[i] = (uint8_t)bus->RXDR;
    }


    if (!wait_flag(bus, I2C_ISR_STOPF, false))
    {
        note_fail(bus, 3);
        abort_transfer(bus);
        return false;
    }

    if (bus->ISR & I2C_ISR_NACKF)
    {
        note_fail(bus, 4);
        abort_transfer(bus);
        return false;
    }

    clear_flags(bus);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC i2c_write : I2C bus (I2C1 ... I2C4 = I2C_TypeDef), chip address 
                       data pointer, number of bytes to write -> bool
  Write (blocking)
 ----------------------------------------------------------------------------------
*/
bool i2c_write (I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint8_t nbytes)
{
    if (nbytes > I2C_MAX_TRANSFER_LEN || (nbytes && !data)) return false;
    if (!bus_idle(bus)) // idle timeout 1000us
    {
        abort_transfer(bus);
        return false;
    } // bus is idle

    // head of the message
    LL_I2C_HandleTransfer(
        bus,
        (uint32_t)(addr << 1), // chip address (shift 1 to put 8 bit type into right place for 7 bit addr)
        LL_I2C_ADDRSLAVE_7BIT,
        nbytes,
        LL_I2C_MODE_AUTOEND,
        LL_I2C_GENERATE_START_WRITE
    );

    // send the content of the message
    for (uint8_t i = 0; i < nbytes; i++)
    {
        if (!wait_flag(bus, I2C_ISR_TXIS, true))
        {
            abort_transfer(bus);
            return false;
        }
        bus->TXDR = data[i];
    }

    if (!wait_flag(bus, I2C_ISR_STOPF, false))
    {
        abort_transfer(bus);
        return false;
    }

    if (bus->ISR & I2C_ISR_NACKF)
    {
        abort_transfer(bus);
        return false;
    }

    clear_flags(bus);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC i2c_probe : I2C bus (I2C1 ... I2C4 = I2C_TypeDef), chip address -> bool
  Returns whether a specific I2C chip is free or not
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t dummy[2];
    return i2c_read(bus, addr, dummy, 2);
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC i2c_probeall : void --> void
  (Developer mode) probes the master's three chips with log UART messages
 ----------------------------------------------------------------------------------
*/
void i2c_probeall (void)
{
    static const struct { uint8_t addr; const char *what; } CHIPS[] = {
        {MCP23017_KNOBS_ADDR, "MCP23017 U7 (K0-K3, B3, INSERT)"},
        {MCP23017_PANEL_ADDR, "MCP23017 U8 (K4, B0-B2, B4-B11)"},
        {MCP4728_ADDR,        "MCP4728  U9 (aux return CVs)"},
    };
    for (uint8_t i = 0; i < sizeof(CHIPS) / sizeof(CHIPS[0]); i++)
    {
        const bool ok = i2c_probe(CONTROLS_I2C_BUS, CHIPS[i].addr);
        printf("\r\n    Probing 0x%02x %s: %s", CHIPS[i].addr, CHIPS[i].what, ok ? "GOOD" : "FAILED or not present");
    }
    printf("\r\n");
}
