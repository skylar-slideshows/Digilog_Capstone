/**
  **********************************************************************************
  * CONTROL VOLTAGE GENERATION ENGINE - DIGILOG CONSOLE
  **********************************************************************************
  * @file control_voltage.h
  * @brief Writes each channel's control voltages (MCP4728 codes) and routing board
  *        switches from its stored control values. The maths is in cv_mapping.c.
  *
  * @author
  * @date
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the “Software”), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

#ifndef CONTROL_VOLTAGE_H
#define CONTROL_VOLTAGE_H

#include <stdint.h>

/**
 * Loads a safe state into the routing board's switches. Call once before the I2C tasks start
 * (init_i2c_scheduler does).
 */
void control_voltage_init (void);

/**
 * Works out a channel's DAC codes and switch bits from its control values, puts the codes in the
 * MCP4728 cache (the I2C scheduler flushes them) and sends any switch changes. Call from the
 * channel's I2C task once per frame, after its control values are updated.
 */
void control_voltage_update (uint8_t channel);

/**
 * Extra attenuation (0 or less, dB) the output VCAs couldn't give at the last update: with the fader
 * very low the rest has to come from the compressor's VCA (cv_mapping.pdf sections 18-19, "E").
 * Not applied yet; for the compressor code.
 */
float control_voltage_comp_extra_db (uint8_t channel);

#endif
