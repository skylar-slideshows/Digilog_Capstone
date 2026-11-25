/**
  **********************************************************************************
  * MCP23017 DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mcp23017.c
  * @brief The bucket controller's driver (Skylar Denno). One bus on the master, so its cache is by address only.
  **********************************************************************************
*/

#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/i2c_driver.h"
#include "CONFIG.h"
#include <stdbool.h>
#include "stm32g474xx.h"

// The master has one bus, so chips are told apart by address alone. Written by the I2C task,
// read by the same task (knobs, buttons) and, for button holds, by the LED task.
static volatile uint8_t mcp23017_value_cache[8]  // 8 possible addresses
                                            [3]; // 2 bytes cached per chip, third byte indicates whether it has yet been polled

/**
 ----------------------------------------------------------------------------------
  @brief PRIVATE mcp23017_init_read : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  SPECIFY REGISTER, NOT FOR THE SCHEDULER (DATA INTENSIVE)
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init_read (I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg, uint8_t *value)
{
    if (!i2c_write(bus, addr, &reg, 1)) return false; // set pointer
    return i2c_read(bus, addr, value, 1);             // read 1 byte
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus, chip's address (0x20, 0x21, 0x22), output -> bool
  Reads both GPIOA and B registers ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_read (I2C_TypeDef *bus, uint8_t addr, uint8_t *out)
{
    return i2c_read(bus, addr, out, 2); //A, B
}

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_poll_to_cache: I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Reads both GPIOA and GOIPB registers in one transaction, and caches them for async
  reading later
 ----------------------------------------------------------------------------------
*/
bool mcp23017_poll_to_cache (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t fresh[2];
    if (!mcp23017_read(bus, addr, fresh)) return false; // on failure, keep the last good snapshot

    (void)bus;
    volatile uint8_t *out_dest = mcp23017_value_cache[addr & 0x07];
    out_dest[0] = fresh[0];
    out_dest[1] = fresh[1];
    out_dest[2] = 1;
    return true;
}

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read_from_cache : I2C bus, chip's address (0x20, 0x21, 0x22) -> 16bit (GPIOB, GPIOA)
  Reads cached values on GPIOA and GOIPB without running an i2c transaction
  Returns an error flag; 1 means it has not yet been polled
 ----------------------------------------------------------------------------------
*/
uint8_t mcp23017_read_from_cache (I2C_TypeDef *bus, uint8_t addr, uint8_t *out)
{
    (void)bus;
    const volatile uint8_t *from = mcp23017_value_cache[addr & 0x07];
    out[0] = from[0];
    out[1] = from[1];

    if (from[2] == 1)
    {
        return 0;
    }

    return 1;
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_write (I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_write(bus, addr, data, 2);
}


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with a no interrupt pin setup.
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init (I2C_TypeDef *bus, uint8_t addr)
{
    uint8_t iocon;
    if (!i2c_probe(bus, addr)) return false; // does chip exist?

    // write both ports seqop (0x0A and 0x0B), disabling auto-increment (see mcp_regs.h)
    {
        uint8_t io[3] = {MCP_IOCONA, IOCON_SEQOP, IOCON_SEQOP};
        if (!i2c_write(bus, addr, io, 3)) return false;
    }

    // write config data to one register at a time, returning false if fail
    if (!mcp23017_write(bus, addr, MCP_IODIRA, 0xFF)) return false; // pin direction: gpio inputs into stm32
    if (!mcp23017_write(bus, addr, MCP_IODIRB, 0xFF)) return false;
    if (!mcp23017_write(bus, addr, MCP_GPPUA, 0xFF)) return false; // use internal 100k pull up res
    if (!mcp23017_write(bus, addr, MCP_GPPUB, 0xFF)) return false;
    if (!mcp23017_write(bus, addr, MCP_IPOLA, 0x00)) return false; // no invert
    if (!mcp23017_write(bus, addr, MCP_IPOLB, 0x00)) return false;

    // check to make sure sequential mode (above) is off
    if (!mcp23017_init_read(bus, addr, MCP_IOCONA, &iocon)) return false;
    if (!(iocon & IOCON_SEQOP)) return false;

    // park the pointer (permanately since seq mode off) @ gpioA
    {
        uint8_t reg = MCP_GPIOA;
        return i2c_write(bus, addr, &reg, 1);
    }
}
