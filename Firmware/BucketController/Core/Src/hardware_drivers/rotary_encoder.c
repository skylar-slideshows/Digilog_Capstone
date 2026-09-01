/**
  **********************************************************************************
  * EC11 ROTARY ENCODER DRIVER AND MOTION DECODER
  **********************************************************************************
  * @file rotary_encoder.c
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

#include <stdbool.h>
#include <stdio.h>
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/mcp23017.h"


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL poll_encoder_state : encoder, encoder state -> bool
  reads BOTH ports of the associated mcp23017 and gets the encoder state from the pins
  @param encoder 
  @param out
 ----------------------------------------------------------------------------------
*/
static bool poll_encoder_state (
    encoder_info_t *encoder, //!< A pointer to location information of the encoder that should be polled
    encoder_state_t *out     //!< A pointer to a struct where the encoder's pin states will be stored
)
{
    uint8_t ports[2];   // [0] = GPIOA, [1] = GPIOB
    if (!mcp23017_read(encoder->i2c_bus, encoder->i2c_addr, ports)) return false;

    out->a = (ports[encoder->a_register - MCP_GPIOA] >> encoder->a_pin) & 1; // subtracts the base to
    out->b = (ports[encoder->b_register - MCP_GPIOA] >> encoder->b_pin) & 1; // MCP_GPIOA both not a typo
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief Determine how a rotary encoder was turned between two polled states
 ----------------------------------------------------------------------------------
*/
static encoder_turn_action_t get_encoder_turn_action (
    encoder_state_t *from, //!< The starting state of the rotary encoder
    encoder_state_t *to    //!< The ending state of the rotary encoder
)
{
    bool a_change = from->a ^ to->a;
    bool b_change = from->b ^ to->b;

    bool matched_starts = from->a == from->b;

    if (!(a_change || b_change))
    {
        return ENCODER_NO_TURN;
    }
    if (a_change && b_change)
    {
        return ENCODER_TURN_ERROR;
    }

    if ((matched_starts && a_change) || (!matched_starts && b_change))
    {
        return ENCODER_TURN_A;
    }

    return ENCODER_TURN_B;
}


encoder_turn_action_t get_encoder_motion (
    encoder_info_t encoder,         // address/location of encoder to poll
    encoder_state_t previous_state, // previous polled state of the encoder
    encoder_state_t *new_state      // pointer to location for saving new polled state of encoder
)
{
    encoder_state_t new_polled_state;
    bool poll_succeeded = poll_encoder_state(&encoder, &new_polled_state);

    if (!poll_succeeded)
    {
        return ENCODER_TURN_ERROR;
    }

    encoder_turn_action_t turn_action = get_encoder_turn_action(&previous_state, &new_polled_state);

    *new_state = new_polled_state;
    return turn_action;
}
