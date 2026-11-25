/**
  **********************************************************************************
  * COMMANDS FROM THE MASTER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/cmd_exec.h
  * @brief Carries out the commands the master sends over SPI (the remote app's commands,
  *        relayed). Channel numbers coming IN are this bucket's local channels 0-3: the master
  *        has already taken off the bucket's offset.
  *
  *   base <n>                 n = first console channel this bucket holds: 1, 5, 9 or 13
  *   ud <ch> <id> <value>     a knob (ids in control_report.h), value 0-65535 (signed: centre 32768)
  *   sw <ch> <id> <value>     a switch: "0"/"1"; input type "00" line, "01" mic, "10" hi-z
  *   chnm <ch> <name>         the channel's name (blank clears it)
  *   sel <ch>:<ch>... / sel - these local channels are selected (the others not) / none here
  *   dump                     report every control again (the app has connected or asked to sync)
  *   ack <seq>                the master received this bucket's packet <seq>
  *   sett ..., md ...         recognised, nothing to do yet
  *
  * Runs in the master link's task. Values go into channel_control_vals under the channel's lock,
  * and into the report snapshot, so a change from the master is never reported back to it.
  **********************************************************************************
*/

#ifndef CMD_EXEC_H
#define CMD_EXEC_H

#include <stdbool.h>
#include <stdint.h>

/** Carries out one command: the text between the %s. False if the verb isn't one of the above
 *  (the framer then knows it's out of step). */
bool cmd_exec (const char *frame);

/** Counts for debugging: commands carried out, and ones recognised but not usable (bad numbers) */
void cmd_exec_counts (uint32_t *done, uint32_t *rejected);

#endif
