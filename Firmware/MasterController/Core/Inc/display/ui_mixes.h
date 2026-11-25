/**
  **********************************************************************************
  * MIXES SCREEN - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/ui_mixes.h
  * @brief The master's screen: the mixes saved on the SD card, always listed.
  *
  *        K4 (the screen knob) moves through the list; pressing it chooses a mix.
  *        The four soft keys sit above B0-B3, the buttons under the screen:
  *          browsing:  B0 SAVE NEW  saves the current mix, titled with the time
  *                     (B3 FORMAT, only when the card has no file system)
  *          chosen:    B0 LOAD   B1 DELETE   B3 CANCEL, their buttons flashing
  *          deleting:  B1 DELETE again to delete it, B3 CANCEL
  *        Turning the knob, or pressing it again, goes back to browsing.
  *        Along the top: the clock, AUTO REC when it's on (flashing while it records),
  *        and at the top right the DAW's timecode (where the playhead is) and one box per
  *        bucket (green there, yellow connecting or quiet, red not connected). Under it, the
  *        card and how many automation moves the mix holds.
  *
  *        LVGL only: runs in the display task.
  **********************************************************************************
*/

#ifndef UI_MIXES_H
#define UI_MIXES_H

/** Builds the screen, mounts the card and reads its list */
void ui_mixes_create (void);

/** Knob and buttons, the card, the clock and the buckets' boxes. Call every pass of the display task. */
void ui_mixes_update (void);

#endif
