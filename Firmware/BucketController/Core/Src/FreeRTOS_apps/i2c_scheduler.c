#include "CONFIG.h"

#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/mcp4728_cache.h"
#include "stm32g474xx.h"

#define MCP23017_1_ADDR 0x20
#define MCP23017_2_ADDR 0x21
#define MCP23017_BUTTON_ADDR 0x22

#define MCP4728_BASE_ADDR 0x60
#define MCP4728S_PER_CHANNEL 5
#define MCP4728S_PER_FRAME 2

#define SUPERFRAME_SIZE 15

typedef struct
{
    uint8_t frame_counter;
    uint8_t last_read_mcp4728_idx;
} i2c_scheduler_state_t;

static const i2c_scheduler_state_t default_scheduler_state = {.frame_counter = 0, .last_read_mcp4728_idx = 0};

static I2C_TypeDef *i2c_busses[CHANNELS] = {I2C1, I2C2, I2C3, I2C4};
static i2c_scheduler_state_t scheduler_states[CHANNELS] = {default_scheduler_state};

static void read_mcp23017s (uint8_t channel)
{
    i2c_scheduler_state_t *state = &(scheduler_states[channel]);
    I2C_TypeDef *i2c_bus = i2c_busses[channel];

    mcp23017_poll_to_cache(i2c_bus, MCP23017_1_ADDR);
    mcp23017_poll_to_cache(i2c_bus, MCP23017_2_ADDR);
    if (state->frame_counter == SUPERFRAME_SIZE - 1)
    {
        mcp23017_poll_to_cache(i2c_bus, MCP23017_BUTTON_ADDR);
    }
}

static void write_mcp4728s (uint8_t channel)
{
    i2c_scheduler_state_t *state = &(scheduler_states[channel]);
    I2C_TypeDef *i2c_bus = i2c_busses[channel];

    for (uint8_t i = 0; i < MCP4728S_PER_FRAME; i++)
    {
        state->last_read_mcp4728_idx = (state->last_read_mcp4728_idx + 1) % MCP4728S_PER_CHANNEL;
        mcp4728_cache_flush_fastWrite(i2c_bus, MCP4728_BASE_ADDR + state->last_read_mcp4728_idx);
    }
}

void start_i2c_frame (uint8_t channel)
{
    read_mcp23017s(channel);
    write_mcp4728s(channel);
    scheduler_states[channel].frame_counter++;
}
