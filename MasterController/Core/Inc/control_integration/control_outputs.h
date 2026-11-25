/**
  **********************************************************************************
  * CONTROL OUTPUTS - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/control_outputs.h
  * @brief What the master's control values drive (the bucket's control_voltage.c, for the
  *        master): the four aux return level CVs (MCP4728 A-D, through its cache), the four
  *        LCV_AUX lines that put each return into the mix, and LCV_INSERT.
  *
  *        A return switched in or out of the mix is first turned down to the bottom of its
  *        range over AUX_SWITCH_FADE_MS, switched, then turned back up, to keep the click down.
  *        (The VCA's bottom is -35.6 dB, not silence, so this softens the switch rather than
  *        hiding it.)
  **********************************************************************************
*/

#ifndef CONTROL_OUTPUTS_H
#define CONTROL_OUTPUTS_H

/**
 * @brief Sets the LCV lines to their power-up state (every return out of the mix, insert out) and
 * makes sure LCV_INSERT and LDAC are outputs (LDAC low, so the DAC's outputs change as written).
 * Call once at start-up, before the I2C task runs.
 */
void control_outputs_init (void);

/**
 * @brief Brings the outputs in line with the control values: DAC codes into the cache (the I2C
 * task sends them), the LCV lines, and any fade in progress. The I2C task calls this every frame.
 */
void control_outputs_update (void);

#endif
