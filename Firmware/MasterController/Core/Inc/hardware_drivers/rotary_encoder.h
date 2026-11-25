/**
  **********************************************************************************
  * ROTARY ENCODER DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file rotary_encoder.h
  * @brief The bucket controller's driver (Skylar Denno). Turns of an EC11, one per contact transition, decoded from
  *        the cached MCP23017 pins.
  **********************************************************************************
*/

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdbool.h>
#include "mcp23017.h"
#include "stm32g474xx.h"
#include "hardware_drivers/button_driver.h"


/**
 ----------------------------------------------------------------------------------
  @brief I2C / pin information on how to reach the rotary encoder

  An I2C bus/address to the GPIO expander that the encoder is on, and the pin information on that GPIO expander
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    I2C_TypeDef *i2c_bus;    //!< i2c bus which the associated GPIO expander is connected to
    uint8_t i2c_addr;        //!< Address of the associated GPIO expander on i2c_bus
    MCP23017_Reg a_register; //!< MCP_GPIOA or MCP_GPIOB
    uint8_t a_pin;           //!< Pin on MCP23017 which encoder output A is connected to
    MCP23017_Reg b_register; //!< MCP_GPIOA or MCP_GPIOB
    uint8_t b_pin;           //!< Pin on MCP23017 which encoder output B is connected to

    button_info_t button_info; //!< The encoder's push button
} encoder_info_t;


/**
 ----------------------------------------------------------------------------------
  @brief A discrete movement of a rotary encoder
 ----------------------------------------------------------------------------------
*/
typedef enum
{
    ENCODER_TURN_A,     //!< Encoder turned in direction of A contact
    ENCODER_TURN_B,     //!< Encoder turned in direction of B contact
    ENCODER_NO_TURN,    //!< Encoder has not turned
    ENCODER_TURN_ERROR, //!< Encoder turned too quickly to tell direction, or doesn't exist
} encoder_turn_action_t;


/**
 ----------------------------------------------------------------------------------
  @brief Get the direction of a rotary encoder's turn since its last update, from cached mcp23017 values.
  The driver keeps the previous pin states of each physical encoder, so call this exactly once per update cycle per encoder.
 ----------------------------------------------------------------------------------
*/
encoder_turn_action_t update_encoder (
    const encoder_info_t *encoder //!< connection configuration of the rotary encoder to read
);

#endif
