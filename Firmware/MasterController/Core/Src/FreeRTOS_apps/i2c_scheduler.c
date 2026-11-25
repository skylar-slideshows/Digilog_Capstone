/**
  **********************************************************************************
  * I2C SCHEDULER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/i2c_scheduler.c
  * @brief See i2c_scheduler.h. From the bucket's i2c_scheduler.c: the same polling, backoff
  *        and refresh, on one bus. The bucket is ticked by TIM4 at 1.5 kHz across four buses;
  *        the master's bus has three chips, so a plain FreeRTOS period is enough.
  *
  *        Bus time per frame at ~390 kHz: two 2-byte expander reads, about 160 us; a DAC write
  *        (only when a level changed, or the 100 ms refresh) adds about 240 us.
  **********************************************************************************
*/

#include "FreeRTOS_apps/i2c_scheduler.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "control_integration/control_interface.h"
#include "control_integration/control_outputs.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/mcp4728_cache.h"
#include <stdbool.h>
#include <stdio.h>

#define MCP23017_FRAMES_UNTIL_RETRY 2000 // a missing expander: try again after this many frames (2 s)
#define MCP4728_FRAMES_UNTIL_RETRY  800  // a missing DAC: likewise
#define MCP4728_FRAMES_UNTIL_REFRESH 100 // rewrite an unchanged DAC every 100 ms, in case it was reset
#define FAILS_BEFORE_BACKOFF        20   // consecutive failures before a present chip is treated as gone

typedef struct
{
    uint8_t addr;
    uint16_t retry_counter; //!< frames until we try a failed chip again
    uint8_t fail_streak;    //!< consecutive failures from a chip that has answered before
    bool seen;              //!< has answered at least once, so a failure is worth reporting
} chip_state_t;

static chip_state_t s_expanders[2] = {
    {.addr = MCP23017_KNOBS_ADDR},
    {.addr = MCP23017_PANEL_ADDR},
};
static chip_state_t s_dac = {.addr = MCP4728_ADDR};
static uint8_t s_dac_refresh_counter;

/** A chip failed: report it (developer mode) and, after enough failures in a row, leave it alone a while */
static void chip_failed (chip_state_t *chip, const char *what, uint16_t frames_until_retry)
{
#if DEVELOPER_MODE
    if (chip->seen)
    {
        printf("\r\n%lu ms  I2C2 %s 0x%02X failed  step %u  ISR 0x%08lX  streak %u\n",
               (unsigned long)HAL_GetTick(), what, chip->addr, i2c_fail_step,
               (unsigned long)i2c_fail_isr, chip->fail_streak + 1);
    }
#else
    (void)what;
#endif
    if (chip->seen && ++(chip->fail_streak) < FAILS_BEFORE_BACKOFF) return;

    chip->fail_streak = 0;
    chip->retry_counter = frames_until_retry;
}

static void poll_expander (chip_state_t *chip)
{
    if (chip->retry_counter > 0)
    {
        chip->retry_counter--;
        return;
    }
    if (mcp23017_poll_to_cache(CONTROLS_I2C_BUS, chip->addr))
    {
        chip->seen = true;
        chip->fail_streak = 0;
        return;
    }
    chip_failed(chip, "expander", MCP23017_FRAMES_UNTIL_RETRY);
}

static void flush_dac (void)
{
    if (s_dac.retry_counter > 0)
    {
        s_dac.retry_counter--;
        return;
    }
    // DAC writes are most of the bus time, so skip it while its outputs haven't changed
    if (mcp4728_cache_is_synced(CONTROLS_I2C_BUS, s_dac.addr) && s_dac_refresh_counter > 0)
    {
        s_dac_refresh_counter--;
        return;
    }
    s_dac_refresh_counter = MCP4728_FRAMES_UNTIL_REFRESH;

    if (mcp4728_cache_flush_fastWrite(CONTROLS_I2C_BUS, s_dac.addr) == 0)
    {
        s_dac.seen = true;
        s_dac.fail_streak = 0;
        return;
    }
    chip_failed(&s_dac, "DAC", MCP4728_FRAMES_UNTIL_RETRY);
}

static void i2c_frame (void)
{
    poll_expander(&s_expanders[0]);
    poll_expander(&s_expanders[1]);
    update_control_values();  // knobs and buttons -> values (and transport -> DAW)
    control_outputs_update(); // values -> DAC cache, LCV lines
    flush_dac();
}

static void i2c_scheduler_task (void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROLS_PERIOD_MS));
        i2c_frame();
    }
}

static uint32_t I2CSchedulerBuffer[512];
static StaticTask_t I2CSchedulerControlBlock;

void init_i2c_scheduler (void)
{
    static const osThreadAttr_t attr = {
        .name = "i2cscheduler",
        .stack_mem = I2CSchedulerBuffer,
        .stack_size = sizeof(I2CSchedulerBuffer),
        .cb_mem = &I2CSchedulerControlBlock,
        .cb_size = sizeof(I2CSchedulerControlBlock),
        .priority = osPriorityBelowNormal, // as on the bucket: USB, the remote and the DAW go first
    };
    if (osThreadNew(i2c_scheduler_task, NULL, &attr) == NULL)
    {
        printf("\r\nI2C scheduler task FAILED to start\n");
        return;
    }
    printf("\r\nI2C scheduler started\n");
}
