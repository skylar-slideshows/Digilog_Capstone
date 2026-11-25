/**
  **********************************************************************************
  * LED HANDLER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/led_handler.c
  * @brief See led_handler.h. Only this task (after start-up) calls into the LED driver.
  **********************************************************************************
*/

#include "FreeRTOS_apps/led_handler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "CONFIG.h"
#include "hardware_drivers/led_driver.h"
#include "control_integration/control_interface.h"
#include <stdio.h>

static void led_task (void *argument)
{
    (void)argument;
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(LED_FRAME_PERIOD_MS));
        update_control_leds();
        led_update();
    }
}

static uint32_t LEDHandlerBuffer[512];
static StaticTask_t LEDHandlerControlBlock;

void init_led_handler (void)
{
    static const osThreadAttr_t attr = {
        .name = "led",
        .stack_mem = LEDHandlerBuffer,
        .stack_size = sizeof(LEDHandlerBuffer),
        .cb_mem = &LEDHandlerControlBlock,
        .cb_size = sizeof(LEDHandlerControlBlock),
        .priority = osPriorityBelowNormal,
    };
    if (osThreadNew(led_task, NULL, &attr) == NULL)
    {
        printf("\r\nLED handler task FAILED to start\n");
        return;
    }
    printf("\r\nLED Rendering Handler Initialized\n");
}
