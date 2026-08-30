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
static bool poll_encoder_state (encoder_info *encoder, encoder_state *out)
{
    uint8_t ports[2];   // [0] = GPIOA, [1] = GPIOB
    if (!mcp23017_read(encoder->i2c_bus, encoder->i2c_addr, ports)) return false;

    out->a = (ports[encoder->a_register - MCP_GPIOA] >> encoder->a_pin) & 1; // subtracts the base to
    out->b = (ports[encoder->b_register - MCP_GPIOA] >> encoder->b_pin) & 1; // MCP_GPIOA both not a typo
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL encoder_turn_action : encoder_state, encoder_state
  // Darya please add the brief description here
  @param from
  @param to
 ----------------------------------------------------------------------------------
*/
static encoder_turn_action get_encoder_turn_action (encoder_state *from, encoder_state *to)
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


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC get_encoder_motion : encoder info, previous encoder state -> encoder rotation direction, new encoder state
  Returns an encoder_turn_action detailing how the rotary encoder has moved
  @param encoder connection configuration of the rotary encoder to read
  @param previous_state last polled state of the rotary encoder
  @param new_state pointer to an encoder_state object in which to store the polled state of the rotary encoder
 ----------------------------------------------------------------------------------
*/
encoder_turn_action get_encoder_motion (
    encoder_info encoder,         // address/location of encoder to poll
    encoder_state previous_state, // previous polled state of the encoder
    encoder_state *new_state      // pointer to location for saving new polled state of encoder
)
{
    encoder_state new_polled_state;
    bool poll_succeeded = poll_encoder_state(&encoder, &new_polled_state);

    if (!poll_succeeded)
    {
        return ENCODER_TURN_ERROR;
    }

    encoder_turn_action turn_action = get_encoder_turn_action(&previous_state, &new_polled_state);

    *new_state = new_polled_state;
    return turn_action;
}
