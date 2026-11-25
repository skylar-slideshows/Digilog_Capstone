/**
  **********************************************************************************
  * I2C SCHEDULER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/i2c_scheduler.h
  * @brief The task that runs the master's controls bus (I2C2), as the bucket's I2C scheduler
  *        does for each channel: every CONTROLS_PERIOD_MS it reads both expanders into their
  *        cache, updates the control values from them, works out the outputs, and sends the
  *        DAC's values when they changed. A chip that stops answering is retried less often
  *        instead of every frame, and reported on the log UART in developer mode.
  **********************************************************************************
*/

#ifndef I2C_SCHEDULER_H
#define I2C_SCHEDULER_H

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC init_i2c_scheduler : void -> void
  Starts the task. Call in USER CODE BEGIN RTOS_THREADS, after init_control_interface().
 ----------------------------------------------------------------------------------
*/
void init_i2c_scheduler (void);

#endif
