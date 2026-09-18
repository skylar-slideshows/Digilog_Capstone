/**
  **********************************************************************************
  * LED HANDLER - DIGILOG CONSOLE
  **********************************************************************************
  * @file Free_RTOS_apps/led_handler.c
  * @brief Runs constant 30fps render and update loop for the display LEDs on the board
  * to constantly reflect / display the current control values as they are changed.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-09-18
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

#include "FreeRTOS_apps/led_handler.h"
#include "hardware_drivers/led_driver.h"
#include "main.h"
#include "stm32g474xx.h"
#include <stdio.h>

void init_led_handler(void)
{
    led_init();

    TIM5->CR1 = TIM_CR1_URS // bit 2: UG/slave-mode resets don't raise UIF
          |     TIM_CR1_ARPE;

    TIM5->EGR = TIM_EGR_UG;         // this sets UIF even with URS=1

    TIM5->SR = ~TIM_SR_UIF;         // write 0 to clear; rc_w0 register
    // enable peripheral side interrupt
    TIM5->DIER |= TIM_DIER_UIE;    

    HAL_NVIC_SetPriority(TIM5_IRQn, 8, 0); // kinda just picked 8 arbitrarily
    HAL_NVIC_EnableIRQ(TIM5_IRQn);

    // starts the cycle counter
    TIM5->CR1 |= TIM_CR1_CEN;

    printf("\r\nLED Rendering Handler Initialized\n");

}


void TIM5_IRQhandler(void)
{
    if (TIM5->SR & TIM_SR_UIF)
    {
        TIM5->SR = ~TIM_SR_UIF; // clear flag, must be first


    }
}
