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
#include "CONFIG.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "cmsis_os2.h"
#include "control_integration/control_interface.h"

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

// ud frames, handed from the ISR to ud_task (which can take the control mutexes)
static uint8_t               ud_storage[512 + 1];
static StaticMessageBuffer_t ud_buffer;
static MessageBufferHandle_t ud_frames;
static uint32_t              ud_stack[512];
static StaticTask_t          ud_tcb;


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
    REMOTE_UART->CR1 |= USART_CR1_TXEIE_TXFNFIE;
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

    const cmd_verb_t verb = verb_of(rx);
    if (verb == CMD_UNKNOWN)
    {
        stat_unknown++;
        return;
    }

    stat_frames++;
    send_ack(rx, rx_len);

    if (verb == CMD_UD) xMessageBufferSendFromISR(ud_frames, rx, rx_len, NULL);
}


/*=============================== INTERRUPT SERVICE ROUTINE ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC uart_cmd_rx_isr : void -> void
  Handles both directions on the remote UART
 ----------------------------------------------------------------------------------
*/
void uart_cmd_rx_isr (void)
{
    /* ---------------- overrun ---------------- */
    if (REMOTE_UART->ISR & USART_ISR_ORE)
    {
        REMOTE_UART->ICR = USART_ICR_ORECF;
        stat_overruns++;
        in_frame = false; // abandon any partial frame
        rx_len   = 0;
    }

    /* ---------------- receive ---------------- */
    if (REMOTE_UART->ISR & USART_ISR_RXNE_RXFNE)
    {
        char c = (char)(REMOTE_UART->RDR & 0xFF); // this read clears RXNE
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
                rx[rx_len] = '\0';
                if (verb_of(rx) == CMD_UNKNOWN)
                {
                    // Not a command, so we are out of step (a lost byte): treat this % as the next frame's opening
                    if (rx_len) stat_unknown++;
                    rx_len = 0;
                }
                else
                {
                    in_frame = false;
                    frame_complete();
                }
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
    if ((REMOTE_UART->CR1 & USART_CR1_TXEIE_TXFNFIE) &&
        (REMOTE_UART->ISR & USART_ISR_TXE_TXFNF))
    {
        if (tx_tail == tx_head)
        {
            REMOTE_UART->CR1 &= ~USART_CR1_TXEIE_TXFNFIE; // ring empty -> stop asking to be interrupted
        }
        else
        {
            REMOTE_UART->TDR = tx[tx_tail];
            tx_tail = (uint16_t)((tx_tail + 1) & (TX_RING_SIZE - 1));
        }
    }
}


/*=============================== UD: SET A KNOB VALUE ================================*/
// %ud <channel> <ctrl_id> <value>% from the remote app: ctrl_id as numbered in its CONFIG.yaml, and value
// is the knob's position across its whole range as 0-65535 (so a centred knob arrives as 32768)

static const struct { uint16_t offset; bool is_signed; } KNOB_VALUES[] = {
    {offsetof(channel_controls, input_gain),                false}, //  0 input
    {offsetof(channel_controls, send_controls[0].gain),     false},  //  1 send_1
    {offsetof(channel_controls, send_controls[0].pan),      true},  //  2 send_1_pan
    {offsetof(channel_controls, send_controls[1].gain),     false},  //  3 send_2
    {offsetof(channel_controls, send_controls[1].pan),      true},  //  4 send_2_pan
    {offsetof(channel_controls, send_controls[2].gain),     false},  //  5 send_3
    {offsetof(channel_controls, send_controls[2].pan),      true},  //  6 send_3_pan
    {offsetof(channel_controls, send_controls[3].gain),     false},  //  7 send_4
    {offsetof(channel_controls, hf_control.gain),           true},  //  8 eq_1_gain (HF)
    {offsetof(channel_controls, hf_control.freq),           false}, //  9 eq_1_freq
    {offsetof(channel_controls, hmf_control.gain),          true},  // 10 eq_2_gain (HMF)
    {offsetof(channel_controls, hmf_control.freq),          false}, // 11 eq_2_freq
    {offsetof(channel_controls, hmf_control.q),             false}, // 12 eq_2_q
    {offsetof(channel_controls, lmf_control.gain),          true},  // 13 eq_3_gain (LMF)
    {offsetof(channel_controls, lmf_control.freq),          false}, // 14 eq_3_freq
    {offsetof(channel_controls, lmf_control.q),             false}, // 15 eq_3_q
    {offsetof(channel_controls, lf_control.gain),           true},  // 16 eq_4_gain (LF)
    {offsetof(channel_controls, lf_control.freq),           false}, // 17 eq_4_freq
    {offsetof(channel_controls, comp_control.in_gain),      true},  // 18 comp_input
    {offsetof(channel_controls, comp_control.attack_time),  false}, // 19 comp_atk
    {offsetof(channel_controls, comp_control.threshold),    true},  // 20 comp_thresh
    {offsetof(channel_controls, comp_control.release_time), false}, // 21 comp_rel
    {offsetof(channel_controls, comp_control.de_ess_amt),   false}, // 22 comp_deess
    {offsetof(channel_controls, comp_control.ratio),        false}, // 23 comp_ratio
    {offsetof(channel_controls, comp_control.out_gain),     false}, // 24 comp_output
    {offsetof(channel_controls, output_pan),                true},  // 25 pan (26, the fader, is not handled yet)
};
#define KNOB_VALUE_COUNT ((long)(sizeof(KNOB_VALUES) / sizeof(KNOB_VALUES[0])))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

static void ud_task (void *argument)
{
    static char frame[CMD_MAX + 1];
    (void)argument;

    for (;;)
    {
        frame[xMessageBufferReceive(ud_frames, frame, CMD_MAX, portMAX_DELAY)] = '\0';

        long ch, id, val;
        if (sscanf(frame, "ud %ld %ld %ld", &ch, &id, &val) != 3) continue;
        if (ch < 0 || ch >= CHANNELS || id < 0 || id >= KNOB_VALUE_COUNT) continue;

        uint8_t *field = (uint8_t *)&channel_control_vals[ch] + KNOB_VALUES[id].offset;
        xSemaphoreTake(channel_control_mutexes[ch], portMAX_DELAY);
        val = CLAMP(val, 0, UINT16_MAX);
        if (KNOB_VALUES[id].is_signed) *(s_scalar_control_t *)field = (s_scalar_control_t)(val - 32768); // centre = 0
        else                           *(u_scalar_control_t *)field = (u_scalar_control_t)val;
        xSemaphoreGive(channel_control_mutexes[ch]);
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

    // 8-byte receive FIFO: at 921600 baud a byte lasts ~11 us, too short to never miss with no FIFO
    HAL_UARTEx_EnableFifoMode(&huart2);

    // Clear anything stale from before we were listening.
    REMOTE_UART->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    (void)REMOTE_UART->RDR;

    ud_frames = xMessageBufferCreateStatic(sizeof(ud_storage) - 1, ud_storage, &ud_buffer);
    static const osThreadAttr_t ud_attr = {
        .name = "uart_cmd", .priority = osPriorityBelowNormal,
        .stack_mem = ud_stack, .stack_size = sizeof(ud_stack),
        .cb_mem = &ud_tcb,     .cb_size = sizeof(ud_tcb),
    };
    osThreadNew(ud_task, NULL, &ud_attr);

    HAL_NVIC_SetPriority(REMOTE_UART_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(REMOTE_UART_IRQn);

    REMOTE_UART->CR1 |= USART_CR1_RXNEIE_RXFNEIE;

    printf("\r\nUART Command Handler Initialized\n");
    
}


/**
 ----------------------------------------------------------------------------------
  @brief Remote UART interrupt vector
 ----------------------------------------------------------------------------------
*/
void REMOTE_UART_IRQHandler (void)
{
    uart_cmd_rx_isr();
}