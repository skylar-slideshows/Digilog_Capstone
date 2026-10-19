/**
  **********************************************************************************
  * FADER POLLER - DIGILOG CONSOLE
  **********************************************************************************
  * @file Free_RTOS_apps/fader_poller.c
  * @brief ...
  *
  * @author 
  * @date 2026-10-10
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026 Skylar Denno
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the "Software"), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

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
        .priority = osPriorityBelowNormal,
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
