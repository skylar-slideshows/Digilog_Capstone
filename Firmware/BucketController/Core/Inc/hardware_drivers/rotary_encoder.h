// TODO: Pretty header

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdbool.h>
#include "mcp23017_driver.h"
#include "stm32g474xx.h"

typedef enum
{
    ENCODER_TURN_A,     // Encoder turned in direction of A contact
    ENCODER_TURN_B,     // Encoder turned in direction of B contact
    ENCODER_NO_TURN,    // Encoder has not turned
    ENCODER_TURN_ERROR, // Encoder turned too quickly or couldn't be read
} encoder_turn_action;

typedef struct
{
    bool a;
    bool b;
} encoder_state;

typedef struct
{
    I2C_TypeDef *i2c_bus;      // i2c bus which the associated GPIO expander is connected to
    uint8_t i2c_addr;          // Address of the associated GPIO expander on i2c_bus
    MCP23017_Reg mcp_register; // Either MCP_GPIOA or MCP_GPIOB
    uint8_t a_pin;             // Pin on MCP23017 which encoder output A is connected to
    uint8_t b_pin;             // Pin on MCP23017 which encoder output B is connected to
    uint8_t btn_pin;           // Pin on the MCP23017
} encoder_info;

/**
 ----------------------------------------------------------------------------------
  @brief get_encoder_motion : encoder info, previous encoder state -> encoder rotation direction, new encoder state
  Returns an encoder_turn_action detailing how the rotary encoder has moved
  @param encoder connection configuration of the rotary encoder to read
  @param previous_state last polled state of the rotary encoder
  @param new_state pointer to an encoder_state object in which to store the polled state of the rotary encoder
 ----------------------------------------------------------------------------------
*/
encoder_turn_action get_encoder_motion (encoder_info encoder, encoder_state previous_state, encoder_state *new_state);

#endif
