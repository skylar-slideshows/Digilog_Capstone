/**
  **********************************************************************************
  * MCP23017 DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mcp23017.h
  * @brief The bucket controller's driver (Skylar Denno). Inputs only: every pin an input, byte mode (IOCON.SEQOP) with the
  *        pointer parked on GPIOA, so each poll is one 2-byte read of GPIOA and GPIOB.
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

typedef uint8_t MCP23017_Reg;

#define MCP_IODIRA ((const MCP23017_Reg)0x00) // direction, port A: 1 = input, POR 0xFF, we will write 0xFF (all inputs)
#define MCP_IODIRB ((const MCP23017_Reg)0x01) // same as above, B
#define MCP_IPOLA  ((const MCP23017_Reg)0x02) // input polarity, port A. 1 = invert the value read from GPIO. we use 0
#define MCP_IPOLB  ((const MCP23017_Reg)0x03) // same as above, B
#define MCP_IOCONA ((const MCP23017_Reg)0x0A) // config; written FIRST, while still sequential, so it can be one 3-byte write
#define MCP_IOCONB ((const MCP23017_Reg)0x0B) // same as above, B
#define MCP_GPPUA  ((const MCP23017_Reg)0x0C) // pull-up enable, port A: 1 = internal ~100k on, we will write 0xFF to use internal
#define MCP_GPPUB  ((const MCP23017_Reg)0x0D) // same as above, B
#define MCP_GPIOA  ((const MCP23017_Reg)0x12) // read = live pin state. WRITE lands in OLAT, not here, pointer is parked here
#define MCP_GPIOB  ((const MCP23017_Reg)0x13) // never addressed directly at runtime: the A/B toggle reaches it from 0x12
#define IOCON_BANK ((const MCP23017_Reg)0x80) // input output command (IOCON) - do not write this bit!

/** @brief 0 on reset - address pointer increments after each byte
1 = byte mode (what we want) no increment so we can access the same address repeatedly
 w/o sending control bytes (reduces traffic on the bus) */
#define IOCON_SEQOP  0x20

#define MCP_POR_IODIR 0xFF // keep power-on reset states
#define MCP_POR_OTHER 0x00


/*=============================== FUNCTIONS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Reads both GPIOA and GOIPB registers in one transaction ALWAYS EXACTLY TWO BYTES.
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017 #1, 0x21 = MCP23017 #2, 0x22 = MCP23017 #3
  @param out pointer to output bytes [2]
 ----------------------------------------------------------------------------------
*/
bool mcp23017_read(I2C_TypeDef *bus, uint8_t addr, uint8_t *out);

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_poll_to_cache: I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Reads both GPIOA and GOIPB registers in one transaction, and caches them for async
  reading later
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017 #1, 0x21 = MCP23017 #2, 0x22 = MCP23017 #3
 ----------------------------------------------------------------------------------
*/
bool mcp23017_poll_to_cache(I2C_TypeDef *bus, uint8_t addr);

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_read_from_cache : I2C bus, chip's address (0x20, 0x21, 0x22) -> 16bit (GPIOB, GPIOA), error flag
  Reads cached values on GPIOA and GOIPB without running an i2c transaction
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017 #1, 0x21 = MCP23017 #2, 0x22 = MCP23017 #3
  @param out pointer to output bytes [2; GPIOA, GPIOB]

  @returns an error flag, 0 indicates success and 1 indicates that the connected gpio expander hasn't yet been polled
 ----------------------------------------------------------------------------------
*/
uint8_t mcp23017_read_from_cache(I2C_TypeDef *bus, uint8_t addr, uint8_t *out);

bool mcp23017_init_read(I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg,uint8_t *out);

/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_write : I2C bus, chip's address (0x20, 0x21, 0x22), register, value -> bool
  Writes one register ALWAYS EXACTLY TWO BYTES.
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017 #1, 0x21 = MCP23017 #2, 0x22 = MCP23017 #3
  @param reg what register to write to (MCP_GPIOA/B)
  @param value ptr to data to write every pin state concurrently (full byte)
               (since not using 23017s for write, no helper to abstract this)
 ----------------------------------------------------------------------------------
*/
bool mcp23017_write(I2C_TypeDef *bus, uint8_t addr, MCP23017_Reg reg, uint8_t value);


/**
 ----------------------------------------------------------------------------------
  @brief mcp23017_init : I2C bus, chip's address (0x20, 0x21, 0x22) -> bool
  Initializes a single MCP23017 chip with no interrupt pin setup.
  @param bus I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
  @param addr 0x20 = MCP23017 #1, 0x21 = MCP23017 #2, 0x22 = MCP23017 #3
 ----------------------------------------------------------------------------------
*/
bool mcp23017_init(I2C_TypeDef *bus, uint8_t addr);

#endif
