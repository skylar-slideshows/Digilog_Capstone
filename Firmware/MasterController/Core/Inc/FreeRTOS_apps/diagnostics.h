/**
  **********************************************************************************
  * DIAGNOSTICS - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/diagnostics.h
  * @brief Says on the log UART (USART2, 921600 baud) how far the master gets: that it
  *        started and why it last reset, how far USB enumeration gets, and where it
  *        stopped if it hit Error_Handler, a HardFault or a FreeRTOS assert.
  *
  *        Hook-up (all in USER CODE sections, so CubeMX keeps them):
  *          main.c                Includes:       #include "FreeRTOS_apps/diagnostics.h"
  *          main.c                2:              diag_boot();
  *          main.c                RTOS_THREADS:   diag_start();
  *          main.c                Error_Handler:  diag_error_handler((uint32_t)__builtin_return_address(0));
  *          stm32g4xx_it.c        Includes:       #include "FreeRTOS_apps/diagnostics.h"
  *          stm32g4xx_it.c        HardFault 0:    diag_hardfault((uint32_t)__builtin_return_address(0));
  *          FreeRTOSConfig.h      1:              see diag_assert below
  **********************************************************************************
*/

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

/** One line on the log UART, from any task. */
void diag_log (const char *text);

/** The start-up line: reset cause and when diagnostics.c was compiled. Call once the UART is set up. */
void diag_boot (void);

/** Starts the task that reports each step of USB enumeration (and when it stalls). */
void diag_start (void);

/** Report and stop. These never return; the report repeats every second or so, so a
 *  terminal opened later still catches it. */
void diag_error_handler (uint32_t caller) __attribute__((noreturn));
void diag_hardfault (uint32_t exc_return) __attribute__((noreturn));

/** For FreeRTOSConfig.h, replacing its configASSERT in USER CODE 1:
 *    void diag_assert (const char *file, int line);
 *    #define configASSERT( x ) if ((x) == 0) { diag_assert(__FILE__, __LINE__); } */
void diag_assert (const char *file, int line) __attribute__((noreturn));

#endif // DIAGNOSTICS_H
