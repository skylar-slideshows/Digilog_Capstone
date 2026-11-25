/**
  **********************************************************************************
  * MCP4728 OUTPUT CACHE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mcp4728_cache.h
  * @brief The bucket controller's DAC cache (Skylar Denno), on the master's one bus.
  **********************************************************************************
*/

#ifndef INC_MCP4728_CACHE_H_
#define INC_MCP4728_CACHE_H_

#include <stdbool.h>
#include <stdint.h>

#include "hardware_drivers/mcp4728.h"
#include "stm32g474xx.h"

/**
 * @brief Writes an output value to the mcp4728 cache
 */
void mcp4728_cache_write (
    I2C_TypeDef *bus,          //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,              //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    uint8_t output_channel,    //!< Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t val //!< 12-bit output value
);

/**
 * @brief Returns whether the chip's outputs already match the cached values
 *
 * False at boot, after any cache write that changes a value, and after a failed flush.
 */
bool mcp4728_cache_is_synced (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr      //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
);

/**
 * @brief Performs a fast write operation on the MCP4728 using cached output values
 *
 * This function performs a fast write operation on the MCP4728 device, updating
 * the DAC output values for all four channels in a single I2C transaction. Gain,
 * voltage reference, power mode options are not configurable. They are not changed
 * in the transaction. Prior values are used. Marks the chip synced on success.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_cache_flush_fastWrite (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr      //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
);

#endif
