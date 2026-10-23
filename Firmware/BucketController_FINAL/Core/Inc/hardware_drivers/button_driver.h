#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdbool.h>
#include "hardware_drivers/mcp23017.h"
#include "stm32g474xx.h"

/**
 * Window after a press during which a second press counts as a double press
 */
#define BUTTON_DOUBLE_PRESS_MS 500

/**
 * @brief Info on the location of a single button
 */
typedef struct
{
    I2C_TypeDef *bus;  //!< I2C bus of the gpio expander which the button is connected to
    uint8_t addr;      //!< I2C address of the gpio expander
    MCP23017_Reg port; //!< Register/Port on the MCP23017 that the button pin is on;
    uint8_t pin;       //!< Pin which the button output is connected to
    bool active_low;   //!< True if the pin reads low while the button is held
} button_info_t;

typedef enum
{
    BUTTON_NO_EVENT,       //!< Button was not newly pressed
    BUTTON_PRESSED,        //!< Button was newly pressed
    BUTTON_DOUBLE_PRESSED, //!< Button was newly pressed again within BUTTON_DOUBLE_PRESS_MS of the previous press
} button_event_t;

/**
 * @brief Get the current hold state of a button based on cached MCP23017 values.
 * Stateless; safe to call from any task. Buttons that don't exist read as not held.
 */
bool get_button_held (const button_info_t *info);

/**
 * @brief Detect new presses of a button, based on cached MCP23017 values.
 * The driver keeps the previous state of each physical button, so call this exactly once per update cycle per button.
 */
button_event_t update_button (const button_info_t *info);

#endif
