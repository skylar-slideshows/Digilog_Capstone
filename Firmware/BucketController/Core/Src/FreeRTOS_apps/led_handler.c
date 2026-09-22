/**
  **********************************************************************************
  * LED HANDLER - DIGILOG CONSOLE
  **********************************************************************************
  * @file led_handler.c
  * @brief Runs constant 30fps render and update loop for the display LEDs on the board
  * to constantly reflect / display the current control values as they are changed.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-09-18
  * @version 1.2
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

#include "FreeRTOS_apps/led_handler.h"
#include "hardware_drivers/led_driver.h"
#include "cmsis_os2.h"
#include "main.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim5;

#define LED_FLAG_TICK 0x01U

static osThreadId_t led_task_handle;

static void led_task (void *arg)
{
    (void)arg;
    HAL_TIM_Base_Start_IT(&htim5);   // start here: scheduler is running now

    for (;;)
    {
        osThreadFlagsWait(LED_FLAG_TICK, osFlagsWaitAny, osWaitForever);
        led_update();
    }
}

void led_handler_notify_from_isr (void)
{
    if (led_task_handle != NULL) osThreadFlagsSet(led_task_handle, LED_FLAG_TICK);
}

void init_led_handler (void)
{
    led_init();

    static const osThreadAttr_t attr = {
        .name = "led", .priority = osPriorityBelowNormal, .stack_size = 512
    };
    led_task_handle = osThreadNew(led_task, NULL, &attr);

    HAL_NVIC_SetPriority(TIM5_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);

    printf("\r\nLED Rendering Handler Initialized\n");
}