/**
  **********************************************************************************
  * I2C DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file i2c_driver.h
  * @brief The bucket controller's register-level I2C driver (Skylar Denno): blocking reads and
  *        writes that poll the peripheral's flags, give up after TIMEOUT_US, and leave the bus
  *        usable after a failure. The bucket runs one bus per channel; the master has one,
  *        I2C2 (CONTROLS_I2C_BUS), so the channel mapping and bit-banging helpers are gone.
  *
  *        On I2C2:
  *          0x20 (U7 MCP23017): K0-K3 A/B/push, B3, INSERT
  *          0x21 (U8 MCP23017): K4 A/B/push, B0-B2, B4-B11
  *          0x60 (U9 MCP4728):  A-D aux return 1-4 level CVs
  **********************************************************************************
*/

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g474xx.h"
#include "CONFIG.h"

/**
 ----------------------------------------------------------------------------------
  @brief i2c_read : bus, chip address, data pointer, number of bytes -> bool
  Read bytes from an i2c chip (blocking). Takes up to I2C_MAX_TRANSFER_LEN bytes.
 ----------------------------------------------------------------------------------
*/
bool i2c_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *data, uint8_t nbytes);

/**
 ----------------------------------------------------------------------------------
  @brief i2c_write : bus, chip address, data pointer, number of bytes -> bool
  Write bytes to an i2c chip (blocking). Takes up to I2C_MAX_TRANSFER_LEN bytes.
 ----------------------------------------------------------------------------------
*/
bool i2c_write (I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint8_t nbytes);

/**
 ----------------------------------------------------------------------------------
  @brief i2c_probe : bus, chip address -> bool
  Whether a chip answers at that address
 ----------------------------------------------------------------------------------
*/
bool i2c_probe (I2C_TypeDef *bus, uint8_t addr);

/**
 ----------------------------------------------------------------------------------
  @brief i2c_init : Set I2C2 to CONTROLS_I2C_TIMING (400 kHz) and start the cycle counter
  the timeouts use. Call once, before any transfer.
 ----------------------------------------------------------------------------------
*/
void i2c_init (void);

/**
 ----------------------------------------------------------------------------------
  @brief i2c_probeall : Developer mode - probe the three chips, results on the log UART
 ----------------------------------------------------------------------------------
*/
void i2c_probeall (void);

/** Why the last transfer failed: the ISR register then, and the step (1 busy before start,
 *  2 data, 3 waiting for STOP, 4 NACK at end) */
extern volatile uint32_t i2c_fail_isr;
extern volatile uint8_t  i2c_fail_step;

#endif
