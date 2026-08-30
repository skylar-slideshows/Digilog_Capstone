/**
  **********************************************************************************
  * EC11 ROTARY ENCODER DRIVER AND MOTION DECODER
  **********************************************************************************
  * @file rotary_encoder.h
  * @brief Functions for the status and motion of the EC11 rotary encoders.
  *
  * @author Darya Petrova (petrov.da@northeastern.edu)
  *         
  * @date 2026-08-27
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026 Darya Petrova
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the “Software”), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdbool.h>
#include "mcp23017.h"
#include "stm32g474xx.h"


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
    ENCODER_TURN_ERROR, //!< Encoder turned too quickly or couldn't be read
} encoder_turn_action_t;


/**
 ----------------------------------------------------------------------------------
  @brief High/Low states of the encoder's A/B pins
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    bool a; //!< High/Low state of the encoder's A pin
    bool b; //!< High/Low state of the encoder's B pin
} encoder_state_t;


/**
 ----------------------------------------------------------------------------------
  @brief I2C / pin information on how to reach the rotary encoder

  An I2C bus/address to the GPIO expander that the encoder is on, and the pin information on that GPIO expander
 ----------------------------------------------------------------------------------
*/
typedef struct
{
    I2C_TypeDef *i2c_bus;      //!< i2c bus which the associated GPIO expander is connected to
    uint8_t i2c_addr;          //!< Address of the associated GPIO expander on i2c_bus
    MCP23017_Reg mcp_register; //!< Either MCP_GPIOA or MCP_GPIOB
    uint8_t a_pin;             //!< Pin on MCP23017 which encoder output A is connected to
    uint8_t b_pin;             //!< Pin on MCP23017 which encoder output B is connected to
} encoder_info_t;


/**
 ----------------------------------------------------------------------------------
  @brief Get the direction of a rotary encoder's turn
 ----------------------------------------------------------------------------------
*/
encoder_turn_action_t get_encoder_motion (
    encoder_info_t encoder,         //!< connection configuration of the rotary encoder to read
    encoder_state_t previous_state, //!< last polled state of the rotary encoder
    encoder_state_t *new_state      //!< pointer to an encoder_state in which to store the polled state of the rotary encoder
);

#endif
