/**
  **********************************************************************************
  * AUTOMATION - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/automation.h
  * @brief Control moves against the DAW's timecode: recorded with AUTO REC, played back to
  *        the buckets, the master's aux returns and the remote app, wherever the playhead is.
  *
  * Recording. AUTO REC (B6) toggles on; when the DAW's transport rolls (play, with or without
  * the DAW's own record) a pass starts and AUTO REC flashes. Every knob and switch a bucket
  * reports, and every aux return level or in-the-mix switch turned on the master, is recorded
  * at the SMPTE time it was moved (a bucket's report time less AUTOMATION_REPORT_LATENCY_MS).
  * When the transport stops (or the playhead jumps back) the pass ends and AUTO REC turns off.
  * A control touched in a pass stops playing back for the rest of it; when the pass is written
  * in, its old moves from the first touch to the end are replaced ("latch").
  *
  * Storage. Moves live on the SD card (mix_store.c), so there's no limit from RAM. RAM holds
  * the moves being recorded (two buffers: when one is three-quarters full it goes to the card,
  * oldest first, while the other fills) and the moves about to play (two windows read ahead).
  * The storage task does all card work (automation_service); this side never waits on the card.
  *
  * Playback. Every BUCKET_POLL_MS the playhead is compared with where it was: moving forward
  * plays the moves in between; a jump (or any move while stopped) has the storage task work out
  * each automated control's value at the new place from the card. Values that change are sent
  * to the buckets that are connected (and to the app), and set on the master's aux returns.
  * The buckets just take commands: they don't know about time.
  *
  * Lanes: per channel, 0-26 the knobs (ud ids), 27-34 the switches (sw ids 0-7); the master
  * section is AUTO_MASTER_CH, with mix_state.h's MIX_MASTER lanes (aux levels, then in-the-mix).
  **********************************************************************************
*/

#ifndef AUTOMATION_H
#define AUTOMATION_H

#include <stdbool.h>
#include <stdint.h>
#include "mix/mix_state.h"
#include "mix/mix_store.h"

#define AUTO_LANES      (MIX_KNOBS + MIX_SWITCHES)   // 35 per channel
#define AUTO_LANE_SW(i) ((uint8_t)(MIX_KNOBS + (i))) // a switch's lane
#define AUTO_MASTER_CH  CONSOLE_CHANNELS             // the master section's lanes
#define AUTO_CHANNELS   (CONSOLE_CHANNELS + 1)

typedef enum
{
    AUTO_REC_OFF = 0,
    AUTO_REC_ARMED,     //!< on, waiting for the transport to roll (lit)
    AUTO_REC_RECORDING, //!< a pass: rolling, recording moves (flashing)
} auto_rec_t;

/** Why AUTO REC is on but not recording yet */
typedef enum
{
    AUTO_WAIT_NONE = 0,
    AUTO_WAIT_PLAY,     //!< the DAW isn't playing
    AUTO_WAIT_TIMECODE, //!< no SMPTE timecode from the DAW (none at all, or bars and beats)
    AUTO_WAIT_SAVING,   //!< the last pass is still being written to the card
} auto_wait_t;

/** Call once before the tasks start */
void automation_init (void);

/** The bucket link's task, every round: starts and ends passes, plays moves */
void automation_tick (void);

/**
 * A control moved by hand at a tick (the bucket link's task for bucket reports, latency
 * AUTOMATION_REPORT_LATENCY_MS; the panel's task for the master section, latency 0).
 * True if it was recorded as a move (a pass is on); false if it's an ordinary change to the mix.
 */
bool automation_record (uint16_t channel, uint8_t lane, uint16_t value, uint32_t tick, uint32_t latency_ms);

/** The master's aux return level (index 0-3) or in-the-mix switch (4-7) changed by hand (the panel's task) */
void automation_master_moved (uint8_t index, uint16_t value);

/** AUTO REC pressed (the panel's task) */
void automation_rec_toggle (void);
auto_rec_t automation_rec_state (void);
auto_wait_t automation_waiting (void);

/** For the screen and autosave */
uint32_t automation_event_count (void);    //!< moves in the mix (on the card, and being recorded)
uint32_t automation_moves_recorded (void); //!< moves recorded since power-up (only goes up)
bool automation_full (void);               //!< moves were lost: recording faster than the card took them, or no card
bool automation_busy (void);               //!< a pass is recording or being written: no saving or loading yet

/*----- the storage task -----*/
/** Card work the engine asked for: buffers to the pass file, a pass into the store, look-ups, read-ahead */
void automation_service (void);
/** A mix was loaded: the store holds its moves (lanes used: one bit per lane per channel) */
void automation_store_loaded (const uint64_t used[AUTO_CHANNELS]);
/** Wakes the storage task (mix_file.c) */
void mix_file_wake (void);

#endif
