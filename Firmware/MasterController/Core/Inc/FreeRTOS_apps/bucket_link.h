/**
  **********************************************************************************
  * BUCKET LINK - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/bucket_link.h
  * @brief The master's end of the link to every bucket controller. One task owns the master
  *        SPI and gives each bucket a window every BUCKET_POLL_MS: a full-duplex exchange of
  *        BUCKET_XCHG_BYTES, commands out and the bucket's packet back in.
  *
  * Out on MOSI (master -> bucket), framed as %...%, 0xFF after the last one:
  *   %ack <seq>%            the last report packet taken from this bucket (0: none yet)
  *   %base <n>%             which console channels it holds: n = 1, 5, 9, 13 ... for CS_Bucket0, 1, 2, 3 ...
  *   %chnm <ch> <name>%     channel names the app has set (local channel 0-3)
  *   %dump%                 report everything again (the app has just connected)
  *   %ud ...% %sw ...%      the app's commands, renumbered to the bucket's local channel 0-3
  *
  * Back on MISO (bucket -> master): $<seq>:<report>;<report>...#<xx>^ then zeros, xx the XOR of
  * the bytes between $ and #. Every window a bucket answers with one of:
  *   $0:hi#xx^              hello: it's on but doesn't know its channels -> the master sends %base%
  *                          (and the names) in its next window
  *   $0:#xx^                keep-alive: nothing to report
  *   $<1-99>:<reports>#xx^  panel changes, CONSOLE channel numbers (0-15), sent again every
  *                          window until acknowledged; repeats are dropped by seq
  * So a bucket that starts late, or restarts, is found within one window of saying hello, and
  * an unplugged one is still asked every BUCKET_RETRY_MS.
  **********************************************************************************
*/

#ifndef BUCKET_LINK_H
#define BUCKET_LINK_H

#include <stdbool.h>
#include <stdint.h>

/** How a bucket is doing, for the screen */
typedef enum
{
    BUCKET_ASKING = 0, //!< since start-up nothing valid yet, still trying (yellow)
    BUCKET_CONNECTING, //!< it said hello; its channel numbers are on the way (yellow)
    BUCKET_PRESENT,    //!< answering, knows its channels (green)
    BUCKET_LOST,       //!< was answering, quiet for BUCKET_LOST_MS (yellow)
    BUCKET_GONE,       //!< nothing for BUCKET_GIVE_UP_MS: taken as unplugged for now (red), still asked
} bucket_status_t;

/** Starts the link task. Call before the scheduler starts, after bucket_spi_init(). */
void bucket_link_start (void);

/** A bucket's status (any task) */
bucket_status_t bucket_link_status (uint8_t bucket);

/** Queues a whole command, "%ud 1 25 40000%" (local channel), for a bucket's next windows. False if its
 *  queue is full. Call from one task only (the remote command relay). */
bool bucket_link_send (uint8_t bucket, const char *frame, uint16_t len);

/** Remembers a channel's name (console channel 0-based) and sends it to its bucket; sent again whenever
 *  that bucket says hello. */
void bucket_link_name (uint16_t channel, const char *name);

/** Asks every bucket to report everything again (the app has connected) */
void bucket_link_dump_all (void);

/** A control a bucket reported moving, for the test screen */
typedef struct
{
    uint16_t channel; //!< console channel, 0-based
    uint8_t id;       //!< control id (ud)
    uint16_t value;   //!< 0-65535
    uint32_t age_ms;  //!< how long ago
} bucket_ctrl_move_t;

/** The controls moved in the last within_ms, newest first, at most max. Returns how many. */
uint8_t bucket_link_recent (bucket_ctrl_move_t *out, uint8_t max, uint32_t within_ms);

/** Counts for debugging */
typedef struct
{
    uint32_t windows; //!< exchanges with this bucket
    uint32_t packets; //!< valid packets received (any kind)
    uint32_t reports; //!< new report packets taken
    uint32_t repeats; //!< report packets seen again (already taken)
    uint32_t bad;     //!< something that started like a packet but didn't check out
    uint32_t hellos;  //!< hello packets
} bucket_link_stats_t;

void bucket_link_counts (uint8_t bucket, bucket_link_stats_t *out);

#endif
