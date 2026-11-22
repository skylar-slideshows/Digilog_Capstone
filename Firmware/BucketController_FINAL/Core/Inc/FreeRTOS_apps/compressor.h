/**
  **********************************************************************************
  * COMPRESSOR - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/compressor.h
  * @brief Runs the four channels' compressors: the comp cards' envelopes in through ADC4, the
  *        gain worked out by comp_engine, out through the STM32's own DACs (COMP_CV0-3) to the
  *        comp cards' scalers and on to the VCAs. See compressor.c for the CubeMX setup it needs.
  **********************************************************************************
*/

#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "control_integration/control_interface.h"

/**
 * Starts the four DAC outputs (and their op-amps) at 0 dB, so a comp card never sees 0 V (its
 * fail-safe, -44 dB). With COMP_LOOP_ENABLED it also starts the sampling (TIM6 -> ADC4 -> DMA) and
 * the compressors. Call once before the scheduler starts.
 */
void compressor_init (void);

/** Hands a channel's compressor knobs (and bypass) to its compressor. Call often; it only recalculates on a change. */
void compressor_set_knobs (uint8_t channel, const comp_control_t *knobs, bool bypassed);

/**
 * E from cv_mapping: the part of a channel's fader + pan + OUTPUT that its main VCA CVs can't reach.
 * The compressor CV adds it (the same VCAs), on top of the gain reduction.
 */
void compressor_set_extra_gain (uint8_t channel, float extra_db);

/** Current gain reduction on a channel, dB (0 = none), for a meter */
float compressor_gain_reduction_db (uint8_t channel);

/** Most CPU cycles one sampling interrupt (all four channels) has taken, to keep an eye on the load */
uint32_t compressor_isr_cycles_max (void);

#endif
