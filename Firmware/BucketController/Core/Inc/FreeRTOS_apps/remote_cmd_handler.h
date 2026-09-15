/**
  **********************************************************************************
  * REMOTE COMMAND HANDLER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/remote_cmd_handler.h
  * @brief 
  *
  * Currently just returns acknoledgement messages that the remote application is
  * set up to look for. Later this needs to HANDLE COMMANDS and parse them thats coming soon.
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-09-14
  **********************************************************************************
*/

#ifndef REMOTE_CMD_HANDLER_H
#define REMOTE_CMD_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#define CMD_MAX 1024 // max command length bytes
#define TX_RING_SIZE 256 // must be pow of 2

// Framing characters
#define CMD_OPEN  '%'
#define CMD_CLOSE '%'
#define ACK_OPEN  '$'
#define ACK_CLOSE '^'

typedef enum {
    CMD_CHNM = 0, // channel name
    CMD_UD, // update control ID
    CMD_SW, // update switch / routing
    CMD_SEL, // channel select
    CMD_SETT, // setting changed
    CMD_MD, // mass data command: md begin, md end. and md 0 = sending a whole channel's params,
    //md 1 = sending console setting data, md 2 = sending 
    CMD_COUNT, // send the physical channel count to remote
    CMD_UNKNOWN = 0xFF
} cmd_verb_t;


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC uart_cmd_init : void -> void
  Priority 5 matches configMAX_SYSCALL_INTERRUPT_PRIORITY
 ----------------------------------------------------------------------------------
*/
void uart_cmd_init(void);


/**
 ----------------------------------------------------------------------------------
  @brief USART2 interrupt vector
 ----------------------------------------------------------------------------------
*/
void uart_cmd_rx_isr(void);


/**
 ----------------------------------------------------------------------------------
  @brief uart_cmd_counts : bytes, frames, unknown, overruns, dropped -> void
  writes diagnostics for debugging
  @param bytes every byte the ISR has read
  @param frames complete %...% frames with a recognised verb
  @param unknown complete frames whose verb was not recognised
  @param overruns USART overrun events (a byte arrived before we read the last)
  @param dropped replies abandoned because the transmit ring was full
 ----------------------------------------------------------------------------------
*/
void uart_cmd_counts(uint32_t *bytes, uint32_t *frames, uint32_t *unknown,
                     uint32_t *overruns, uint32_t *dropped);

#endif

