#include <stdbool.h>
#include <stdint.h>

#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/i2c_driver.h"
#include "CONFIG.h"
#include "stm32g4xx_hal.h"

/** Previous state of one physical button, so presses can be detected between updates */
typedef struct
{
    bool held;
    bool awaiting_second_press;
    uint32_t last_press_ms;
} button_cache_t;

static button_cache_t button_cache[CHANNELS]  // one I2C bus per channel
                                  [8]         // 8 possible expander addresses per channel
                                  [2]         // GPIOA / GPIOB
                                  [8];        // 8 pins per port

static bool button_exists (const button_info_t *info)
{
    return i2c_bus_index(info->bus) < CHANNELS && info->pin < 8 && (uint8_t)(info->port - MCP_GPIOA) < 2;
}

bool get_button_held (const button_info_t *info)
{
    if (!button_exists(info))
    {
        return false;
    }

    uint8_t gpio_bits[2];
    mcp23017_read_from_cache(info->bus, info->addr, gpio_bits);
    const bool pin_high = (gpio_bits[info->port - MCP_GPIOA] >> info->pin) & 1;
    return pin_high != info->active_low;
}

button_event_t update_button (const button_info_t *info)
{
    if (!button_exists(info))
    {
        return BUTTON_NO_EVENT;
    }
    button_cache_t *cache = &button_cache[i2c_bus_index(info->bus)][info->addr & 0x07][info->port - MCP_GPIOA][info->pin];

    const bool held = get_button_held(info);
    const bool pressed = held && !cache->held;
    cache->held = held;

    if (!pressed)
    {
        return BUTTON_NO_EVENT;
    }

    const uint32_t now = HAL_GetTick();
    if (cache->awaiting_second_press && now - cache->last_press_ms < BUTTON_DOUBLE_PRESS_MS)
    {
        cache->awaiting_second_press = false;
        return BUTTON_DOUBLE_PRESSED;
    }

    cache->awaiting_second_press = true;
    cache->last_press_ms = now;
    return BUTTON_PRESSED;
}
