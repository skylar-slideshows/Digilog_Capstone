/**
  **********************************************************************************
  * CONSOLE TEST SCREEN - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/ui_console_test.h
  * @brief A plain test screen: white text on black listing the controls being moved on any
  *        bucket, one line each, "CHANNEL x: CTRL y%" (x the console channel 1-16, y the
  *        control's 16-bit value as a percentage of 0xFFFF); and along the bottom, sixteen
  *        boxes 01-16 for the channels: green when their bucket is there, yellow while it's
  *        being asked or connecting (or something's off), red when the master has taken it as
  *        unplugged (it's still asked, and turns green when it answers).
  **********************************************************************************
*/

#ifndef UI_CONSOLE_TEST_H
#define UI_CONSOLE_TEST_H

/** Builds the screen. Call once from the display task, after LVGL is set up. */
void ui_console_test_create (void);

/** Brings it up to date (moves and bucket status). Call from the display task, every few tens of ms. */
void ui_console_test_update (void);

#endif
