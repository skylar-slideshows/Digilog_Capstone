/**
  **********************************************************************************
  * CHANNEL DISPLAYS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/channel_display.h
  * @brief Task that keeps each channel's screen showing that channel's state, laid out
  *        as in the index.html mockup (ui/channel_screen.h). It checks the control values
  *        about 30 times a second and redraws only the parts that changed.
  **********************************************************************************
*/

#ifndef CHANNEL_DISPLAY_H
#define CHANNEL_DISPLAY_H

/**
 * Starts the channel display task. Call in main.c USER CODE BEGIN RTOS_THREADS, after
 * displays_init() and init_control_interface() have run.
 */
void init_channel_display (void);

#endif
