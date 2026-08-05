/*
  ******************************************************************************
  * FILE : app.cpp
  * BRIEF : flash led2 (ensure cpp working)
  ******************************************************************************
  *
  * AUTHORS : Skylar,
  *
  * LAST UPDATE : 2024-06-05
  *
  ******************************************************************************
*/


#include "main.h"
#include "cmsis_os2.h"
#include "app.hpp"

#ifndef LD2_Pin
#define LD2_Pin        GPIO_PIN_5
#define LD2_GPIO_Port  GPIOA
#endif


/******************************************************************************/


namespace
{

class Blinker
{

public:

    constexpr Blinker(GPIO_TypeDef* port, uint16_t pin, uint32_t period_ms)
        : port_{port}, pin_{pin}, period_ms_{period_ms} {}

    [[noreturn]] void run()
    {

        uint32_t next = osKernelGetTickCount();
        for (;;)
        {

            HAL_GPIO_TogglePin(port_, pin_);
            next += period_ms_;
            osDelayUntil(next);

        }

    }

private:

    GPIO_TypeDef* port_;
    uint16_t      pin_;
    uint32_t      period_ms_;

};

Blinker heartbeat{LD2_GPIO_Port, LD2_Pin, 50};

void blink_thread(void*) { heartbeat.run(); }

const osThreadAttr_t blink_attr =

{
    .name = "blink",
    .stack_size = 512,
    .priority = osPriorityLow,

};

}


/******************************************************************************/


void app_main(void)
{

    osThreadNew(blink_thread, nullptr, &blink_attr);

    for (;;)
    {
        osDelay(1000);
    }

}