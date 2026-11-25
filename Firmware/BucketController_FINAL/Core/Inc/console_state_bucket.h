/**
  **********************************************************************************
  * PARAMETER SAVING AND DATA INFRASTRUCTURE - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file console_state_bucket.h
  * @brief Bucket identity and channel names. Where this bucket sits in the whole console. The console has four buckets of four
  *        channels; the master tells each bucket which ones it holds by sending "%base n%" over
  *        SPI on that bucket's chip select: n = 1 on CS_Bucket0 (console channels 01-04), 5 on
  *        CS_Bucket1 (05-08), 9 on CS_Bucket2 (09-12), 13 on CS_Bucket3 (13-16). It repeats this
  *        every second, so a bucket that restarts learns it again straight away.
  *
  *        Two numberings come out of it:
  *          - console channel NUMBER, 1-16, what the screens show ("01" ... "16")
  *          - console channel INDEX, 0-15, what the remote app and the master use in commands
  *        Until the master has said, neither is known: the screens show "--" and nothing is
  *        reported to the master.
  *
  *        Also kept here: the channel names the master sends ("%chnm <ch> <name>%").
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

#ifndef CONSOLE_STATE_H
#define CONSOLE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#define CHANNEL_NAME_MAX 20 // as the master: what fits on a screen's title strip

/** Sets the first console channel this bucket holds (1, 5, 9, 13 ...), from "%base n%". Anything that
 *  isn't the start of a group of CHANNELS, or that would go past channel 99, is ignored. Returns true
 *  if it was taken. */
bool console_state_set_base (uint32_t first);

/** True once this bucket knows its channels: from the master, or channels 01-04 in solo mode */
bool console_state_known (void);

/** No master answered at start-up: this bucket runs on its own as channels 01-04 (boot_screen.c decides).
 *  It keeps listening: a %base% from the master later ends solo mode and gives the real channels. */
void console_state_set_solo (void);

/** True in solo mode */
bool console_state_solo (void);

/** True once the master has told this bucket its channels (only then are panel changes reported) */
bool console_state_from_master (void);

/** Which bucket this is: 0 on CS_Bucket0 ... 3 on CS_Bucket3, or 0xFF until known */
uint8_t console_bucket (void);

/** A local channel's console channel number, 1-16 (for the screens), or 0 until known */
uint8_t console_channel_number (uint8_t local);

/** A local channel's console channel index, 0-15 (the app's and master's numbering), or -1 until known */
int16_t console_channel_index (uint8_t local);

/** Stores a channel's name from "%chnm <ch> <name>%": surrounding spaces dropped, at most
 *  CHANNEL_NAME_MAX characters, anything outside plain ASCII replaced with '?', empty clears it */
void console_state_set_name (uint8_t local, const char *name);

/** Copies a channel's name ("" if none) */
void console_state_name (uint8_t local, char out[CHANNEL_NAME_MAX + 1]);

#endif
