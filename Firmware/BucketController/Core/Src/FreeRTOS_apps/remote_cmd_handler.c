/**
  **********************************************************************************
  * REMOTE COMMAND HANDLER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/remote_cmd_handler.c
  * @brief 
  *
  * Currently just returns acknoledgement messages that the remote application is
  * set up to look for. Later this needs to 
  *
  * @author Skylar Denno (denno.o@northeastern.edu)
  * @date 2026-09-15
  **********************************************************************************
*/

#include "FreeRTOS_apps/remote_cmd_handler.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;


/*=============================== COMMAND DEFNS ================================*/
static const char *const VERBS[CMD_COUNT] = {
    "chnm",   // CMD_CHNM  channel name
    "ud",     // CMD_UD    update DAC
    "sw",     // CMD_SW    switch
    "sel",    // CMD_SEL   select
    "sett",   // CMD_SETT  setting
    "md",     // CMD_MD    mode
};


/*=============================== STATE ================================*/

// receive
static char rx[CMD_MAX + 1];
static volatile uint16_t rx_len;
static volatile bool in_frame;

// transmit
static volatile uint8_t tx[TX_RING_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;

// diagnostic values
static volatile uint32_t stat_bytes;
static volatile uint32_t stat_frames;
static volatile uint32_t stat_unknown;
static volatile uint32_t stat_overruns;
static volatile uint32_t stat_dropped;


/*=============================== TRANSMIT ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL tx_push : byte -> bool
  Adds one byte to the ring, and returns false if the ring is full.
 ----------------------------------------------------------------------------------
*/
static bool tx_push (uint8_t b)
{
    uint16_t next = (uint16_t)((tx_head + 1) & (TX_RING_SIZE - 1));
    if (next == tx_tail) return false;          // full

    tx[tx_head] = b;
    tx_head = next;
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL tx_start : void -> void
  enables the transmit-empty interrupt so the ring starts draining
 ----------------------------------------------------------------------------------
*/
static void tx_start (void)
{
    USART2->CR1 |= USART_CR1_TXEIE_TXFNFIE;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL send_ack : command text -> void
  Queues "$ack <command>^" for transmission
 ----------------------------------------------------------------------------------
*/
static void send_ack (const char *text, uint16_t len)
{
    // 5 for "$ack ", 1 for the closing '^'
    uint16_t need = (uint16_t)(len + 6);
    uint16_t free_space = (uint16_t)((tx_tail - tx_head - 1) & (TX_RING_SIZE - 1));

    if (need > free_space)
    {
        stat_dropped++;
        return;
    }

    tx_push(ACK_OPEN);
    tx_push('a'); tx_push('c'); tx_push('k'); tx_push(' ');
    for (uint16_t i = 0; i < len; i++) tx_push((uint8_t)text[i]);
    tx_push(ACK_CLOSE);

    tx_start();
}


/*=============================== PARSING ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL verb_of : command string -> cmd_verb_t
  Takes the first whitespace-delimited word and matches it whole with the defined command verbs
 ----------------------------------------------------------------------------------
*/
static cmd_verb_t verb_of (const char *s)
{
    uint16_t len = 0;
    while (s[len] && s[len] != ' ' && s[len] != '\t') len++;
    if (len == 0) return CMD_UNKNOWN;

    for (uint8_t i = 0; i < CMD_COUNT; i++)
    {
        if (strlen(VERBS[i]) == len && strncmp(s, VERBS[i], len) == 0)
            return (cmd_verb_t)i;
    }
    return CMD_UNKNOWN;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL frame_complete : void -> void
  Handle closing % receipt
 ----------------------------------------------------------------------------------
*/
static void frame_complete (void)
{
    rx[rx_len] = '\0';

    if (rx_len == 0) return;

    if (verb_of(rx) == CMD_UNKNOWN)
    {
        stat_unknown++;
        return;
    }

    stat_frames++;
    send_ack(rx, rx_len);
}


/*=============================== INTERRUPT SERVICE ROUTINE ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC uart_cmd_rx_isr : void -> void
  Handles both directions on USART2
 ----------------------------------------------------------------------------------
*/
void uart_cmd_rx_isr (void)
{
    /* ---------------- overrun ---------------- */
    if (USART2->ISR & USART_ISR_ORE)
    {
        USART2->ICR = USART_ICR_ORECF;
        stat_overruns++;
        in_frame = false; // abandon any partial frame
        rx_len   = 0;
    }

    /* ---------------- receive ---------------- */
    if (USART2->ISR & USART_ISR_RXNE_RXFNE)
    {
        char c = (char)(USART2->RDR & 0xFF); // this read clears RXNE
        stat_bytes++;

        if (c == CMD_OPEN && !in_frame)
        {
            in_frame = true; // start a frame
            rx_len   = 0;
        }
        else if (in_frame)
        {
            if (c == CMD_CLOSE)
            {
                in_frame = false;
                frame_complete();
            }
            else if (rx_len >= CMD_MAX)
            {
                // Over-long frame, abandon and wait for the next opening %, so
                // one bad frame cannot desynchronise the % intepretation.
                in_frame = false;
                rx_len   = 0;
            }
            else
            {
                rx[rx_len++] = c;
            }
        }
        // else: junk between frames, discard
    }

    // TRANSMIT
    if ((USART2->CR1 & USART_CR1_TXEIE_TXFNFIE) &&
        (USART2->ISR & USART_ISR_TXE_TXFNF))
    {
        if (tx_tail == tx_head)
        {
            USART2->CR1 &= ~USART_CR1_TXEIE_TXFNFIE; // ring empty -> stop asking to be interrupted
        }
        else
        {
            USART2->TDR = tx[tx_tail];
            tx_tail = (uint16_t)((tx_tail + 1) & (TX_RING_SIZE - 1));
        }
    }
}


/*=============================== PUBLIC FUNCTIONS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC uart_cmd_counts
  just writes some diagnostics lol debugging :c
 ----------------------------------------------------------------------------------
*/
void uart_cmd_counts (uint32_t *bytes, uint32_t *frames, uint32_t *unknown,
                      uint32_t *overruns, uint32_t *dropped)
{
    if (bytes) *bytes = stat_bytes;
    if (frames) *frames = stat_frames;
    if (unknown) *unknown = stat_unknown;
    if (overruns) *overruns = stat_overruns;
    if (dropped) *dropped = stat_dropped;
}


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC uart_cmd_init : void -> void
  Priority 5 matches configMAX_SYSCALL_INTERRUPT_PRIORITY
 ----------------------------------------------------------------------------------
*/
void uart_cmd_init (void)
{
    rx_len   = 0;
    in_frame = false;
    tx_head  = 0;
    tx_tail  = 0;

    stat_bytes = stat_frames = stat_unknown = stat_overruns = stat_dropped = 0;

    // Clear anything stale from before we were listening.
    USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    (void)USART2->RDR;

    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    USART2->CR1 |= USART_CR1_RXNEIE_RXFNEIE;

    printf("\r\nUART Command Handler Initialized\n");
}


/**
 ----------------------------------------------------------------------------------
  @brief USART2 interrupt vector
 ----------------------------------------------------------------------------------
*/
void USART2_IRQHandler (void)
{
    uart_cmd_rx_isr();
}
