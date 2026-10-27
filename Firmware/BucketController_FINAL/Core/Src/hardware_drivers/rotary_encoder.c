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
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/i2c_driver.h"
#include "CONFIG.h"


/** Last polled pin states of one physical encoder, so motion can be detected between updates */
typedef struct
{
    bool seeded; //!< False until the first update, which only records the pin states
    bool a;
    bool b;
} encoder_cache_t;

static encoder_cache_t encoder_cache[CHANNELS]  // one I2C bus per channel
                                    [8]         // 8 possible expander addresses per channel
                                    [2]         // GPIOA / GPIOB of the encoder's A pin
                                    [8];        // 8 pins per port, indexed by the encoder's A pin


/**
 ----------------------------------------------------------------------------------
  @brief Determine how a rotary encoder was turned between two polled states
 ----------------------------------------------------------------------------------
*/
static encoder_turn_action_t get_encoder_turn_action (
    const encoder_cache_t *from, //!< The starting state of the rotary encoder
    const encoder_cache_t *to    //!< The ending state of the rotary encoder
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


encoder_turn_action_t update_encoder (const encoder_info_t *encoder)
{
    const uint8_t channel = i2c_bus_index(encoder->i2c_bus);
    if (channel >= CHANNELS || encoder->a_pin >= 8 || (uint8_t)(encoder->a_register - MCP_GPIOA) >= 2)
    {
        return ENCODER_TURN_ERROR;
    }
    encoder_cache_t *cache = &encoder_cache[channel][encoder->i2c_addr & 0x07][encoder->a_register - MCP_GPIOA][encoder->a_pin];

    uint8_t ports[2]; // [0] = GPIOA, [1] = GPIOB

    if(mcp23017_read_from_cache(encoder->i2c_bus, encoder->i2c_addr, ports) == 1){
        return ENCODER_NO_TURN; // In case the GPIO expander hasn't yet been polled
    }

    // subtracts the base to MCP_GPIOA both not a typo
    const encoder_cache_t polled = {
        .seeded = true,
        .a = (ports[encoder->a_register - MCP_GPIOA] >> encoder->a_pin) & 1,
        .b = (ports[encoder->b_register - MCP_GPIOA] >> encoder->b_pin) & 1,
    };

    const encoder_turn_action_t turn_action = cache->seeded ? get_encoder_turn_action(cache, &polled) : ENCODER_NO_TURN;

    *cache = polled;
    return turn_action;
}
