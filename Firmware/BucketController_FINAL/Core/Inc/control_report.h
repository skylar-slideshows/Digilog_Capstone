/**
  **********************************************************************************
  * CONTROL REPORTS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file control_report.h
  * @brief What the bucket tells the master: every control the user changes on the panel.
  *
  * A snapshot of what was last reported is kept for every control. The master link's task
  * looks at each channel's controls every LINK_SCAN_MS and marks anything that differs from
  * the snapshot (knobs, the fader, switches, select), taking the new value into the snapshot.
  * Changes that came from the master go straight into the snapshot (report_remote_...), so
  * they're never sent back. A control being turned is reported with its latest value only.
  *
  * Reports use the same words as the commands, with the CONSOLE channel index (0-15, the app's
  * numbering), not the bucket's local one:
  *     ud <ch> <id> <value>    value 0-65535; signed controls as stored + 32768 (centre 32768)
  *     sw <ch> <id> <value>    "0"/"1"; input type as "00" line, "01" mic, "10" hi-z
  *     sel <ch>                this channel was selected on the panel ("sel -": none here)
  *
  * Knob ids (ud):  0 input gain   1 send 1   2 send 1 pan   3 send 2   4 send 2 pan   5 send 3
  *   6 send 3 pan   7 send 4   8 HF gain   9 HF freq   10 HMF gain   11 HMF freq   12 HMF Q
  *   13 LMF gain   14 LMF freq   15 LMF Q   16 LF gain   17 LF freq   18 comp input   19 attack
  *   20 threshold   21 release   22 (de-ess: not used)   23 ratio   24 comp output   25 pan   26 fader
  * Switch ids (sw): 0 input type   1 phase   2 +48V   3 HPF   4 solo   5 mute   6 insert   7 pre-fader insert
  *
  * Everything here runs in the master link's task, so it needs no locks of its own.
  **********************************************************************************
*/

#ifndef CONTROL_REPORT_H
#define CONTROL_REPORT_H

#include <stdbool.h>
#include <stdint.h>
#include "control_integration/control_interface.h"

#define REPORT_KNOBS    27 // ud ids 0-26
#define REPORT_SWITCHES 8  // sw ids 0-7
#define KNOB_FADER      26

/** A knob id's value as the protocol carries it (0-65535; signed controls + 32768). False for a bad id. */
bool report_knob_value (const channel_controls *v, uint8_t id, uint16_t *out);

/** Sets a knob id from a protocol value. False for a bad id. (The caller holds the channel's lock.) */
bool report_set_knob (channel_controls *v, uint8_t id, uint16_t value);

/** A switch id's value: 0/1, or for the input type 0 line, 1 mic, 2 hi-z (the app's order) */
uint8_t report_switch_value (const channel_controls *v, uint8_t id);

/** Compares one channel's controls (a copy taken under its lock) with what was last reported,
 *  and marks what changed. The first look at a channel marks everything, so the master learns the
 *  whole state once the bucket knows its channels. */
void report_scan (uint8_t channel, const channel_controls *now);

/** The local channel selected on this bucket's panel (-1 for none), marked if it changed */
void report_scan_select (int8_t selected);

/** Values the master just set: taken as already reported, so they don't go back */
void report_remote_knob (uint8_t channel, uint8_t id, uint16_t value);
void report_remote_switch (uint8_t channel, uint8_t id, uint8_t value);
void report_remote_select (int8_t selected);

/** Marks one switch to be reported as it is (e.g. the master asked for +48V on a line input, refused) */
void report_mark_switch (uint8_t channel, uint8_t id);

/** The master's "%dump%": every control on every channel, and the selection, reported again */
void report_dump (void);

/** True if anything is waiting to be reported */
bool report_pending (void);

/**
 * Writes as many waiting reports as fit into out, separated by ';' (no terminator counted), using
 * console channel indexes from first_index (the index of local channel 0). Those reports are then
 * no longer waiting. Picks up where the last call stopped, so a knob being turned can't crowd the
 * others out. Returns the length written (0: nothing waiting, or nothing fits). out gets a '\0'.
 */
uint16_t report_build (char *out, uint16_t room, int16_t first_index);

#endif
