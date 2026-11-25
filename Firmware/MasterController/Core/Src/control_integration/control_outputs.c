/**
  **********************************************************************************
  * CONTROL OUTPUTS - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/control_outputs.c
  * @brief See control_outputs.h. Runs in the I2C task, once per frame (CONTROLS_PERIOD_MS).
  **********************************************************************************
*/

#include "control_integration/control_outputs.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/mcp4728_cache.h"
#include "CONFIG.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} lcv_pin_t;

static const lcv_pin_t LCV_AUX[AUX_RETURNS] = {
    {LCV_AUX1_GPIO_Port, LCV_AUX1_Pin}, // PE2: return 1 into the mix (ADG1436 U12 on the analog board)
    {LCV_AUX2_GPIO_Port, LCV_AUX2_Pin}, // PE3
    {LCV_AUX3_GPIO_Port, LCV_AUX3_Pin}, // PE4
    {LCV_AUX4_GPIO_Port, LCV_AUX4_Pin}, // PE5
};

typedef enum
{
    FADE_NONE, //!< the level follows its knob
    FADE_DOWN, //!< turning down before the switch
    FADE_UP,   //!< switched: turning back up to the knob
} fade_state_t;

typedef struct
{
    fade_state_t state;
    bool in_mix;    //!< what LCV_AUXn is driving now
    uint32_t steps; //!< frames into the current fade
} return_output_t;

static return_output_t s_returns[AUX_RETURNS];
static bool s_insert_on;

/** Knob value (0-65535) -> 12-bit DAC code: the DAC's whole 0-2.048 V range is the VCA's control range */
static uint16_t level_code (uint16_t level)
{
    return (uint16_t)(level >> 4);
}

static void set_pin (GPIO_TypeDef *port, uint16_t pin, bool high)
{
    HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void control_outputs_init (void)
{
    // LCV_INSERT may not be set up in CubeMX yet (it isn't in MasterController.ioc): make it an output here
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_WritePin(LCV_INSERT_GPIO_Port, LCV_INSERT_Pin, GPIO_PIN_RESET);
    GPIO_InitTypeDef g = {0};
    g.Pin = LCV_INSERT_Pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCV_INSERT_GPIO_Port, &g);

    // LDAC low: the DAC's outputs change as each write ends
    HAL_GPIO_WritePin(LDAC_GPIO_Port, LDAC_Pin, GPIO_PIN_RESET);

    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        set_pin(LCV_AUX[ret].port, LCV_AUX[ret].pin, false);
        s_returns[ret] = (return_output_t){.state = FADE_NONE, .in_mix = false, .steps = 0};
        mcp4728_cache_write(CONTROLS_I2C_BUS, MCP4728_ADDR, ret, 0); // bottom until the first update
    }
    s_insert_on = false;
}

void control_outputs_update (void)
{
    xSemaphoreTake(master_control_mutex, portMAX_DELAY);
    const master_controls v = master_control_vals;
    xSemaphoreGive(master_control_mutex);

    const uint32_t fade_steps = (AUX_SWITCH_FADE_MS + CONTROLS_PERIOD_MS - 1U) / CONTROLS_PERIOD_MS;

    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        return_output_t *r = &s_returns[ret];
        const uint16_t target = level_code(v.aux_return_level[ret]);
        uint16_t code = target;

        if (r->state == FADE_NONE && r->in_mix != v.aux_return_to_mix[ret])
        {
            r->state = FADE_DOWN;
            r->steps = 0;
        }

        switch (r->state)
        {
            case FADE_DOWN:
                r->steps++;
                code = (fade_steps == 0U || r->steps >= fade_steps) ? 0U
                                                                    : (uint16_t)(target - (uint32_t)target * r->steps / fade_steps);
                if (code == 0U)
                {
                    // at the bottom: switch now. A fade straight back to where it was if the knob changed its mind.
                    r->in_mix = v.aux_return_to_mix[ret];
                    set_pin(LCV_AUX[ret].port, LCV_AUX[ret].pin, r->in_mix);
                    r->state = FADE_UP;
                    r->steps = 0;
                }
                break;
            case FADE_UP:
                r->steps++;
                if (fade_steps == 0U || r->steps >= fade_steps)
                {
                    r->state = FADE_NONE;
                    code = target;
                }
                else
                {
                    code = (uint16_t)((uint32_t)target * r->steps / fade_steps);
                }
                break;
            case FADE_NONE:
            default:
                break;
        }

        mcp4728_cache_write(CONTROLS_I2C_BUS, MCP4728_ADDR, ret, code);
    }

    if (v.insert != s_insert_on)
    {
        s_insert_on = v.insert;
        set_pin(LCV_INSERT_GPIO_Port, LCV_INSERT_Pin, s_insert_on);
    }
}
