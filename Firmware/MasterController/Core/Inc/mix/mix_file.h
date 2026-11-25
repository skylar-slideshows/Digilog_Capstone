/**
  **********************************************************************************
  * MIX FILES - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_file.h
  * @brief Mixes on the SD card (SPI3, the DISK pins): .dgl files, the JSON the remote app also
  *        saves and loads (app: state.py), so a mix moves either way between the console and the app.
  *
  *        A file (version 4) holds every control of every channel ("Channel 1" ...): name, knobs,
  *        toggles, input type; the master section (aux return levels, in the mix or not); and the
  *        automation, every move in time order (mix_file.c has the layout). Saving writes the mix as
  *        it stands; loading puts every value into the mix, sends it to the buckets that are
  *        connected (the rest are skipped, their values kept), and its moves replace the mix's.
  *
  *        A storage task owns the card: these functions hand it the job and wait for it, so call
  *        them from a task that can wait (the display task). It also does automation's card work
  *        and the autosave: "<mix> (autosave).dgl", AUTOSAVE_PERIOD_MS after the last save if the
  *        mix changed, or once AUTOSAVE_AFTER_MOVES moves have been recorded, whichever is first.
  *
  *        Long file names are on (FatFs LFN, up to 64 characters). The clock is set by the remote
  *        app ("time" command); until then it counts from 00-00-00 at power-up.
  **********************************************************************************
*/

#ifndef MIX_FILE_H
#define MIX_FILE_H

#include <stdbool.h>
#include <stdint.h>

#define MIX_NAME_MAX 40 // file names listed, with ".dgl"
#define MIX_LIST_MAX 48 // the most mixes listed

typedef enum
{
    MIX_OK = 0,
    MIX_NO_CARD,       //!< no card answered
    MIX_NOT_FORMATTED, //!< a card, but no FAT file system on it
    MIX_DISK_ERROR,    //!< the card stopped answering, or a file couldn't be read or written
    MIX_NOT_FOUND,     //!< no such mix
    MIX_BAD_FILE,      //!< not a mix this console can read
    MIX_EXISTS,        //!< (save) no free title this second
    MIX_BUSY,          //!< AUTO REC is recording, or writing its last pass: no saving or loading yet
} mix_result_t;

typedef struct
{
    uint16_t channels;   //!< channels in the file
    uint16_t sent;       //!< of those, sent to a connected bucket
    uint16_t skipped;    //!< on buckets that aren't connected: kept in the mix, not sent
    uint32_t moves;      //!< automation moves loaded
    uint32_t moves_lost; //!< moves out of time order (skipped), or a card error
    bool old_layout;     //!< automation in version 3's layout: not loaded (re-record it)
} mix_load_report_t;

/** Starts the storage task (call before the scheduler starts) */
void mix_file_start (void);

/** One pass of the storage task's work (the task calls it; host tests too) */
void mix_file_service (void);

/** Mounts the card if it isn't. Returns its state (MIX_OK when it can be used). */
mix_result_t mix_file_mount (void);

/** The .dgl files on the card, sorted by name. Returns how many (0 if the card can't be read). */
uint16_t mix_file_list (char names[][MIX_NAME_MAX + 1], uint16_t max);

/** Saves the current mix as a new file titled with the time ("14-05-33.dgl"); its name comes back in saved_name */
mix_result_t mix_file_save_new (char saved_name[MIX_NAME_MAX + 1]);

/** Loads a mix: every value into the mix, to the buckets that are connected, and its moves */
mix_result_t mix_file_load (const char *name, mix_load_report_t *report);

/** Deletes a mix */
mix_result_t mix_file_delete (const char *name);

/** Formats the card (FAT), only when mix_file_mount() says MIX_NOT_FORMATTED. Everything on it is lost. */
mix_result_t mix_file_format (void);

/** Autosaves so far (it goes up by one each time), and the last one's file name */
uint32_t mix_file_autosaves (char name[MIX_NAME_MAX + 1]);

/** A short description of a result, for the screen */
const char *mix_file_result_text (mix_result_t r);


/*=============================== CLOCK ================================*/

typedef struct
{
    uint16_t year;   //!< 2000-2099
    uint8_t month;   //!< 1-12
    uint8_t day;     //!< 1-31
    uint8_t hour, minute, second;
} mix_time_t;

/** Sets the clock (the remote app's "time" command). Safe from any task. */
void mix_clock_set (const mix_time_t *t);

/** True once the clock has been set */
bool mix_clock_is_set (void);

/** The time now: the clock, or time since power-up (from 2000-01-01 00:00:00) until it's set */
void mix_clock_now (mix_time_t *t);

/** The time now in FatFs's format, for get_fattime() (FATFS/App/app_fatfs.c) */
uint32_t mix_clock_fattime (void);

#endif
