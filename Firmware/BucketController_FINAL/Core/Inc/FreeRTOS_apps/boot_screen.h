/**
  **********************************************************************************
  * START-UP SCREEN - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/boot_screen.h
  * @brief What the bucket shows while it starts:
  *   1. the DIGILOG picture on all four screens, the screens fading in over BOOT_FADE_IN_MS; the
  *      knob LEDs stay dark;
  *   2. held until the master has told this bucket its channels ("%base n%"), or until
  *      BOOT_MASTER_TIMEOUT_MS after start-up if it doesn't;
  *   3. the screens fade out over BOOT_FADE_OUT_MS.
  *   With no master, the bucket goes into solo mode: channels 01-04, nothing over SPI (no remote
  *   app, no reports), and every channel screen says NO MASTER in red along the bottom.
  *   Then the channel screens are drawn and faded in, and the knob LEDs come straight on.
  *
  * The controls, control voltages and switches work throughout; only the screens and the LEDs
  * are held.
  **********************************************************************************
*/

#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <stdbool.h>

/** Call before the scheduler starts, after displays_init() and led_init(): keeps the screens and the knob
 *  LEDs dark, and starts the start-up task. */
void boot_screen_init (void);

/** True once the start-up screen is over and the channel screens may draw */
bool boot_screen_done (void);

#endif
