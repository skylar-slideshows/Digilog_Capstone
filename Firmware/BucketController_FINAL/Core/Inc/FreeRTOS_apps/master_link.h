/**
  **********************************************************************************
  * MASTER LINK - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/master_link.h
  * @brief This bucket's end of the SPI link to the master controller: the bucket is the SPI
  *        slave, one of four on a shared bus, with its own chip select. It replaces the old
  *        remote-app link over USART2 (USART2 now only carries the debug log).
  *
  * Every chip-select window is an exchange (BUCKET_CONTROL_SYNC.md, section 4):
  *   MOSI, master -> bucket: commands framed as %...% (see cmd_exec.h), anything between frames
  *        ignored (the master pads with 0xFF). From the master's first message this bucket learns
  *        which console channels it holds ("%base 5%": channels 05-08).
  *   MISO, bucket -> master: one packet, zero padded to LINK_XCHG_BYTES:
  *            $<seq>:<report>;<report>;...#<xx>^      e.g.  $17:ud 5 25 40000;sw 4 5 1#24^
  *        seq 1-99 (wraps 99 -> 1, never 0); xx = XOR of every byte after '$' up to (not
  *        including) '#', as two uppercase hex digits. All zeros when there's nothing to say.
  *        Reports are the panel changes, with CONSOLE channel numbers 0-15 (see control_report.h).
  *   The master answers with "%ack <seq>%" in a later exchange. Until it does, the same packet is
  *   sent again every exchange (the master drops repeats by seq); after it, the next packet is
  *   built from whatever has changed since.
  *
  * Hardware: SPI1 slave, mode 0, 8 bit, MSB first. PB3 SCK, PB5 MOSI, PB4 MISO, PB6 chip select.
  *   - The chip select comes in on EXTI6, both edges (priority 1, no FreeRTOS calls). Falling:
  *     the SPI is told it's selected and PB4 starts driving MISO. Rising: PB4 lets go of MISO
  *     (the other buckets share it) and the next packet is loaded for the next window.
  *   - DMA2 channel 1: SPI1 RX, circular, into a ring the link task reads every millisecond.
  *   - DMA2 channel 2: SPI1 TX, the packet, armed before the window opens so its first bit is on
  *     MISO at the first clock. SPI1 is reset each time it's re-armed, which empties its FIFOs if
  *     the last window was shorter than the packet.
  *   All set up here in registers; CubeMX's SPI1 settings are overridden.
  **********************************************************************************
*/

#ifndef MASTER_LINK_H
#define MASTER_LINK_H

#include <stdint.h>

/** Sets up SPI1, its DMA and the chip-select interrupt, and starts the link task. Call once, before
 *  the scheduler starts and after init_control_interface(). */
void master_link_init (void);

/** Solo mode (no master at start-up): lets go of SPI1 and the chip-select interrupt, and the link task
 *  ends. Nothing is received or sent over SPI after this. */
void master_link_stop (void);

/** The master acknowledged packet <seq> (from "%ack <seq>%"); called by cmd_exec */
void master_link_ack (uint8_t seq);

/** Counts for debugging */
typedef struct
{
    uint32_t windows;     //!< chip-select windows (exchanges) seen
    uint32_t bytes;       //!< bytes received
    uint32_t frames;      //!< commands carried out or rejected
    uint32_t unknown;     //!< %...% frames that weren't commands
    uint32_t packets;     //!< report packets built
    uint32_t acked;       //!< report packets the master acknowledged
} master_link_stats_t;

void master_link_counts (master_link_stats_t *out);

#endif
