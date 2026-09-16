#include <stdbool.h>
#include <stdint.h>
#include "hardware_drivers/fader_driver.h"

#define FADER_POWER_COEFFICIENT 4

static uint16_t get_physical_fader_position (fader_info_t *info)
{
    // TODO
}

static bool is_fader_touched (fader_info_t *info)
{
    // TODO
}

static inline int32_t clamp_mult (int32_t a, int32_t b)
{
    int32_t x = a * b;
    if (a != 0 && x / a != b)
    {
        if (a > 0 && b > 0)
        {
            return INT32_MAX;
        }
        if (a < 0 && b < 0)
        {
            return INT32_MAX;
        }
        return INT32_MIN;
    }
    return x;
}

static int32_t get_motor_power (fader_info_t *info, fader_state_t *state)
{
    const int32_t where_it_is = get_physical_fader_position(info);
    const int32_t where_it_isnt = state->position;

    const int32_t deviation = where_it_isnt - where_it_is;
    return clamp_mult(deviation, FADER_POWER_COEFFICIENT);
}

void update_fader (fader_info_t *info, fader_state_t *old_state, fader_state_t *new_state)
{
    fader_state_t new_state_i = *old_state;
    if (is_fader_touched(info))
    {
        new_state_i.movement_mode = FADER_UNPOWERED;
        new_state_i.position = get_physical_fader_position(info);
    }
    else
    {
        new_state_i.movement_mode = FADER_MOVE_OR_HOLD_TARGET;
        get_motor_power(info, &new_state_i);
        // TODO: Write to motor hardware
    }

    *new_state = new_state_i;
}
