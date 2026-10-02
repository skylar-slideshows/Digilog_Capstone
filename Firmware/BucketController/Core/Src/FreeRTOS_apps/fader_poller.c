#include "CONFIG.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "hardware_drivers/fader_driver.h"
#include "portmacro.h"
#include "projdefs.h"
#include "stm32g474xx.h"
#include <stdbool.h>

#include "FreeRTOS_apps/fader_poller.h"

extern TIM_HandleTypeDef htim4; // from main.c

static osThreadId_t fader_scheduler_task_handle = NULL;

static uint32_t FaderSchedulerBuffer[512];
static StaticTask_t FaderSchedulerControlBlock;

static void fader_poller_task (void *arg)
{
    (void)arg;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        poll_faders();
    }
}

void init_fader_poller (void)
{
    const osThreadAttr_t attr = {
        .name = "faderscheduler",
        .stack_mem = FaderSchedulerBuffer,
        .stack_size = sizeof(FaderSchedulerBuffer),
        .cb_mem = &FaderSchedulerControlBlock,
        .cb_size = sizeof(FaderSchedulerControlBlock),
        .priority = osPriorityNormal,
    };

    fader_scheduler_task_handle = osThreadNew(fader_poller_task, NULL, &attr);

    HAL_NVIC_SetPriority(TIM4_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    HAL_TIM_Base_Start_IT(&htim4);
}

void fader_poller_trigger_frame (void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (fader_scheduler_task_handle != NULL)
    {
        vTaskNotifyGiveFromISR(fader_scheduler_task_handle, &higher_priority_task_woken);
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
