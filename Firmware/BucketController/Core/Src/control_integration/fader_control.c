#include "fader_control.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/fader_driver.h"
#include "hardware_state_sets.h"
#include "hardware_sets.h"
#include <stdint.h>

static inline s_scalar_control_t uint16_to_s_scalar (uint16_t in)
{
    return (s_scalar_control_t)(in ^ 0x8000); // XOR MSB
}

void update_channel_fader_hardware (channel_control_io_state *state, channel_control_io_t *io)
{
    update_fader(&(io->fader), &(state->fader_state), &(state->fader_state));
}

void update_channel_fader_values (channel_controls *vals, channel_control_io_state *state, channel_control_io_t *io)
{
    vals->output_gain = uint16_to_s_scalar(state->fader_state.position);
}

void init_fader_controls (uint8_t channel, channel_control_io_state *state, channel_control_io_t *io)
{
    io->fader = (fader_info_t){}; // TODO
    state->fader_state = DEFAULT_FADER_STATE;
}
