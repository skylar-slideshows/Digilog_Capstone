#include "CONFIG.h"

#include "FreeRTOS.h"
#include "control_integration/control_interface.h"
#include "task.h"
#include "cmsis_os2.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/mcp4728_cache.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32g474xx.h"
#include "stm32g4xx_hal.h"
// #include <stdio.h>
#include <string.h>

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

static void start_i2c_frame (uint8_t channel)
{
    // printf("chan %d\n", channel);
    read_mcp23017s(channel);

    update_control_values(channel);
    update_control_leds(channel);
    // TODO: DAC values

    write_mcp4728s(channel);
    scheduler_states[channel].frame_counter++;
}

static void i2c_scheduler_task (void *arg)
{
    uint8_t channel = *(uint8_t *)arg;
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        start_i2c_frame(channel);
    }
}

static osThreadId_t i2c_scheduler_task_handles[4];
static uint8_t i2c_sched_task_args[4];

static uint32_t I2CSchedulerBuffers[4][128];
static StaticTask_t I2CSchedulerControlBlocks[4];

extern TIM_HandleTypeDef htim4; // from main.c

void init_i2c_scheduler (void)
{
    const char *names[4] = {
        "i2cscheduler1",
        "i2cscheduler2",
        "i2cscheduler3",
        "i2cscheduler4",
    };

    for (uint8_t i = 0; i < CHANNELS; i++)
    {
        const osThreadAttr_t attr = {
            .name = names[i],
            .stack_mem = I2CSchedulerBuffers[i],
            .stack_size = sizeof(I2CSchedulerBuffers[i]),
            .cb_mem = &(I2CSchedulerControlBlocks[i]),
            .cb_size = sizeof(I2CSchedulerControlBlocks[i]),
            .priority = osPriorityRealtime,
        };
        i2c_sched_task_args[i] = i;
        i2c_scheduler_task_handles[i] = osThreadNew(i2c_scheduler_task, &(i2c_sched_task_args[i]), &attr);
    }

    HAL_NVIC_SetPriority(TIM4_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    HAL_TIM_Base_Start_IT(&htim4);
}

void i2c_scheduler_trigger_frame (void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i2c_scheduler_task_handles[i] == NULL)
        {
            continue;
        }

        vTaskNotifyGiveFromISR(i2c_scheduler_task_handles[i], NULL);
    }
}
