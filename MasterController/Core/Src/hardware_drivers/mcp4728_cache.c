/**
  **********************************************************************************
  * MCP4728 OUTPUT CACHE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mcp4728_cache.c
  * @brief The bucket controller's DAC cache (Skylar Denno): values are written to the cache
  *        whenever they're worked out, and the I2C task sends a chip's four values in one fast
  *        write only when they changed (or for a periodic refresh). One bus on the master, so
  *        chips are told apart by address alone.
  **********************************************************************************
*/

#include <stdbool.h>
#include <stdint.h>
#include "CONFIG.h"
#include "hardware_drivers/mcp4728_cache.h"
#include "hardware_drivers/mcp4728.h"
#include "stm32g474xx.h"

static volatile mcp4728_output_value_t mcp4728_value_cache[8]  // 8 possible addresses
                                                           [4]; // 4 12-bit outputs cached per chip

// true when the chip's outputs match the cache. starts false so every chip gets written once at boot
static volatile bool mcp4728_cache_synced[8];

void mcp4728_cache_write (
    I2C_TypeDef *bus,          // I2C bus
    uint8_t addr,              // 7-bit addr of the DAC e.g 0x60
    uint8_t output_channel,    // Output channel; 0 -> A, 1 -> B, 2 -> C, 3 -> D
    mcp4728_output_value_t val // 12-bit output value
)
{
    (void)bus;
    if (output_channel > 3) return;
    if (mcp4728_value_cache[addr & 0x07][output_channel] == val) return;
    mcp4728_value_cache[addr & 0x07][output_channel] = val;
    mcp4728_cache_synced[addr & 0x07] = false;
}

bool mcp4728_cache_is_synced (I2C_TypeDef *bus, uint8_t addr)
{
    (void)bus;
    return mcp4728_cache_synced[addr & 0x07];
}

uint8_t mcp4728_cache_flush_fastWrite (I2C_TypeDef *bus, uint8_t addr)
{
    volatile mcp4728_output_value_t *outs = mcp4728_value_cache[addr & 0x07];
    // mark synced *before* reading the values: if another task writes the cache mid-flush,
    // it clears the flag again and the new value goes out on the next flush
    mcp4728_cache_synced[addr & 0x07] = true;
    const uint8_t code = mcp4728_fastWrite(bus, addr, outs[0], outs[1], outs[2], outs[3]);
    if (code != 0) mcp4728_cache_synced[addr & 0x07] = false;
    return code;
}
