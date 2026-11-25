/**
  **********************************************************************************
  * REMOTE COMMAND HANDLER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/remote_cmd_handler.h
  * @brief The remote app's protocol over the master's USB serial port: the same %...% frames
  *        and "$ack ...^" replies the buckets use on their UART. Knob (ud) and switch (sw)
  *        commands are forwarded to the bucket that owns the channel, over the master SPI.
  **********************************************************************************
*/

#ifndef REMOTE_CMD_HANDLER_H
#define REMOTE_CMD_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#define CMD_MAX 1024 // max command length bytes

// Framing characters (the same as the bucket's)
#define CMD_OPEN  '%'
#define CMD_CLOSE '%'
#define ACK_OPEN  '$'
#define ACK_CLOSE '^'

typedef enum
{
    CMD_CHNM = 0, // channel name
    CMD_UD,       // update control ID
    CMD_SW,       // update switch / routing
    CMD_SEL,      // channel select
    CMD_SETT,     // setting changed
    CMD_MD,       // mass data command
    CMD_COUNT,
    CMD_UNKNOWN = 0xFF
} cmd_verb_t;

typedef struct
{
    uint32_t bytes;        // every byte received from the remote app
    uint32_t frames;       // complete %...% frames with a recognised verb (each one acknowledged)
    uint32_t unknown;      // complete frames whose verb was not recognised
    uint32_t ack_dropped;  // acknowledgements abandoned because the USB transmit buffer was full
    uint32_t queue_full;   // ud/sw commands not taken (and so not acknowledged: the app resends)
    uint32_t relayed;      // ud/sw commands sent to a bucket
    uint32_t relay_failed; // ud/sw commands not sent: channel missing or out of range, or SPI error
} remote_cmd_stats_t;


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC remote_cmd_init : void -> void
  Creates the relay task. Call once before the scheduler starts, after bucket_spi_init().
  Receiving needs no setup: TinyUSB calls tud_cdc_rx_cb() when the remote app sends.
  A second call does nothing.
 ----------------------------------------------------------------------------------
*/
void remote_cmd_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC remote_cmd_counts : stats -> void
  Copies the diagnostic counters, for debugging.
 ----------------------------------------------------------------------------------
*/
void remote_cmd_counts (remote_cmd_stats_t *out);

#endif // REMOTE_CMD_HANDLER_H
