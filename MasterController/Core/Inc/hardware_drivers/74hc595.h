/**
  **********************************************************************************
  * SHIFT REGISTER DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file 74hc595.h
  * @brief The latch of the LED 74HCT595 chain. Data and clock go out over SPI2 (led_driver.c);
  *        this only pulses the latch (PB14, LED_LAT) once a frame has been shifted out.
  *
  *        From the bucket controller's driver (Skylar Denno). The master has no MCP4728
  *        address-programming shift registers (its one DAC keeps the factory address and its
  *        LDAC is a plain GPIO), so only the LED half is here.
  **********************************************************************************
*/

#ifndef HC595_H
#define HC595_H

#include "stm32g4xx.h"
#include <stdbool.h>

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC latch_out : Pulse the LED latch to update the register outputs
 ----------------------------------------------------------------------------------
*/
void latch_out (void);

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC led_shiftreg_init : Set up the LED latch pin (data and clock go out over SPI)
 ----------------------------------------------------------------------------------
*/
void led_shiftreg_init (void);

#endif
