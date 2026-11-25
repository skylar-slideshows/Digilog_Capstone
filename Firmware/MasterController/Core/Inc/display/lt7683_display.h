/**
  **********************************************************************************
  * DISPLAY - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/lt7683_display.h
  * @brief The 7" 1024 x 600 display (BuyDisplay ER-TFT070A2-6-5633, LT7683 controller) and
  *        LVGL on top of it.
  *
  *        Levetop's LT768x library (LT768.c, LT768_Lib.c, if_port.c) drives the LT7683 over
  *        the 16-bit 8080 bus on the FMC. LVGL draws into a 4-line buffer in the MCU's RAM
  *        and each finished strip is written to the LT7683's frame memory through its
  *        "active window + memory write" (MPU16_16bpp_Memory_Write). Everything runs in one
  *        FreeRTOS task, so LVGL is only ever called from there.
  *
  *        The screen is the mixes on the SD card (ui_mixes.c), so the SD card (FatFs) is used
  *        from this task too.
  **********************************************************************************
*/

#ifndef LT7683_DISPLAY_H
#define LT7683_DISPLAY_H

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC display_start : void -> void
  Starts the display task: it resets and sets up the LT7683, turns the backlight on,
  starts LVGL and shows the mixes screen. Call in USER CODE BEGIN RTOS_THREADS (TIM1,
  whose channel 3 is the backlight, is set up by CubeMX; led_init() may change its period).
 ----------------------------------------------------------------------------------
*/
void display_start (void);

#endif
