/**
  **********************************************************************************
  * LED HANDLER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/led_handler.h
  * @brief The task that redraws the LEDs from the control values every LED_FRAME_PERIOD_MS,
  *        as the bucket's LED handler does (the bucket ticks it from TIM5 at 120 Hz).
  **********************************************************************************
*/

#ifndef LED_HANDLER_H
#define LED_HANDLER_H

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC init_led_handler : void -> void
  Starts the LED renderer. Call in USER CODE BEGIN RTOS_THREADS, after init_control_interface().
 ----------------------------------------------------------------------------------
*/
void init_led_handler (void);

#endif
