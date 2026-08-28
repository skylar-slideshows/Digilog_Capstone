#include <stdbool.h>

#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/mcp23017_driver.h"

static bool poll_encoder_state (encoder_info *encoder, encoder_state *out)
{
    uint8_t read_val;
    bool read_succeeded = mcp23017_read(encoder->i2c_bus, encoder->i2c_addr, encoder->mcp_register, &read_val);

    if (!read_succeeded) return false;

    out->a = (read_val >> encoder->a_pin) & 1;
    out->b = (read_val >> encoder->b_pin) & 1;

    return true;
}

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
