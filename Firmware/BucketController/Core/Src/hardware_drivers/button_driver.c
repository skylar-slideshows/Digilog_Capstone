#include <stdbool.h>

#include "hardware_drivers/button_driver.h"

bool get_button_state (button_info_t *info)
{
    uint8_t gpio_bits[2];
    mcp23017_read_from_cache(info->bus, info->addr, gpio_bits);
    if (info->port == MCP_GPIOA)
        return (gpio_bits[0] >> info->pin) & 1;
    else
        return (gpio_bits[1] >> info->pin) & 1;
}

