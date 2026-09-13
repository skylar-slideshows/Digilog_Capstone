#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "hardware_drivers/mcp23017.h"
#include "stm32g474xx.h"

/**
 * @brief Info on the location of a single button
 */
typedef struct
{
    I2C_TypeDef *bus;  //!< I2C bus of the gpio expander which the button is connected to
    uint8_t addr;      //!< I2C address of the gpio expander
    MCP23017_Reg port; //!< Register/Port on the MCP23017 that the button pin is on;
    uint8_t pin;       //!< Pin which the button output is connected to
} button_info_t;

/**
 * @brief Get the current hold state of a button based on cached MCP23017 values
 */
bool get_button_state (button_info_t *info);

#endif
