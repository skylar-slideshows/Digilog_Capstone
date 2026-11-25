/**
  **********************************************************************************
  * MIX STATE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_state.h
  * @brief The latest value of every control on every console channel, as the master last
  *        heard it: from the buckets' reports, from the remote app's commands, or from a mix
  *        loaded off the SD card. Nothing is ever polled; a control nobody has touched holds
  *        the bucket's own start-up value (control_interface.c in the bucket firmware).
  *        This is what "save mix" writes.
  *
  *        Values are as the protocol carries them: knobs 0-65535 (centred controls 32768 at
  *        centre), switches 0/1, the input type 0 line, 1 mic, 2 hi-z.
  *        Safe from any task (short critical sections).
  **********************************************************************************
*/

#ifndef MIX_STATE_H
#define MIX_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "CONFIG.h"

#define MIX_KNOBS    27 // ud ids 0-26 (26: fader)
#define MIX_SWITCHES 8  // sw ids 0-7 (0: input type)
#define MIX_MASTER   (2 * AUX_RETURNS) // the master section: aux return levels 0-3, then each in the mix (0/1) 4-7

typedef struct
{
    uint16_t knob[MIX_KNOBS];
    uint8_t sw[MIX_SWITCHES];
    bool sel;
} mix_channel_t;

/** Every channel to the bucket's start-up values. Call once at start-up. */
void mix_state_init (void);

/** A knob (ud id) or switch (sw id) of a console channel (0-based). Out-of-range ids are ignored. */
void mix_state_set_knob (uint16_t channel, uint8_t id, uint16_t value);
void mix_state_set_switch (uint16_t channel, uint8_t id, uint8_t value);

/** A bucket reported its selection: `selected` (console channel) or -1 for none of its channels */
void mix_state_set_bucket_select (uint8_t bucket, int16_t selected);

/** A copy of one channel's values */
void mix_state_get (uint16_t channel, mix_channel_t *out);

/** The master section's static values (what a mix saves for the aux returns): see MIX_MASTER */
void mix_state_set_master (uint8_t index, uint16_t value);
uint16_t mix_state_get_master (uint8_t index);

/** Counts every change to the mix (values, names, moves), for autosave: a change since a save shows as a new count */
uint32_t mix_state_changes (void);
void mix_state_changed (void);

/** The bucket firmware's start-up value of a knob / switch */
uint16_t mix_state_default_knob (uint8_t id);
uint8_t mix_state_default_switch (uint8_t id);

#endif
