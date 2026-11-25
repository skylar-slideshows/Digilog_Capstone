/**
  **********************************************************************************
  * MOVE STORE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_store.h
  * @brief The automation moves of the mix on the SD card, so there's no limit from RAM.
  *
  *        AUTOMOVE.DAT  every move, 8 bytes each, in time order (the mix's automation)
  *        AUTOPASS.DAT  the pass being recorded: moves come here from RAM as it fills
  *        AUTOMOVN.DAT  the next AUTOMOVE.DAT while it's being built (a pass merged in,
  *                      or a mix loaded), then renamed over it
  *        Started empty at power-up. Only the storage task touches these (FatFs).
  **********************************************************************************
*/

#ifndef MIX_STORE_H
#define MIX_STORE_H

#include <stdbool.h>
#include <stdint.h>

/** One move: a control (channel 0-15, or the master section; its lane) and its value at t */
typedef struct
{
    uint32_t t;      //!< ms from 00:00:00:00 on the DAW's timecode
    uint8_t ch;      //!< console channel, or AUTO_MASTER_CH
    uint8_t lane;    //!< automation.h
    uint16_t value;
} store_ev_t;

/** A control touched in a pass: its old moves from `from` to the pass's end are replaced */
typedef struct
{
    uint8_t ch, lane;
    uint32_t from;
} store_touch_t;

/** No moves: the files go. Call once the card is mounted. */
void store_reset (void);
/** Moves in the store */
uint32_t store_count (void);
/** Up to max moves from index on. False if the card failed. */
bool store_read (uint32_t index, store_ev_t *out, uint16_t max, uint16_t *got);
/** The first move later than ms (store_count() if none) */
bool store_first_after (uint32_t ms, uint32_t *index);

/** Building a new store from moves in time order (a mix being loaded); close(true) puts it in place */
bool store_new_open (void);
bool store_new_put (const store_ev_t *e);
bool store_new_close (bool keep);

/** The pass file: started empty, moves appended in time order */
bool store_pass_reset (void);
bool store_pass_append (const store_ev_t *e, uint16_t n);
uint32_t store_pass_count (void);

/**
 * A pass done: its moves (the pass file, then tail[0..n_tail)) go into the store. Each touched
 * control's old moves from its first touch to end_ms are dropped. The store is rebuilt in time order.
 */
bool store_merge_pass (const store_touch_t *touched, uint8_t n_touched, uint32_t end_ms, const store_ev_t *tail, uint16_t n_tail);

#endif
