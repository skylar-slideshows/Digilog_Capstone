/**
  **********************************************************************************
  * MASTER LINK - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/master_link.c
  * @brief See master_link.h.
  **********************************************************************************
*/

#include "FreeRTOS_apps/master_link.h"
#include "FreeRTOS_apps/cmd_exec.h"
#include "control_integration/control_interface.h"
#include "control_report.h"
#include "console_state_bucket.h"
#include "CONFIG.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert((LINK_RX_RING & (LINK_RX_RING - 1)) == 0, "LINK_RX_RING must be a power of two");
_Static_assert(LINK_XCHG_BYTES >= 32, "a packet needs room for a few reports");

#define LINK_SPI   SPI1
#define RX_DMA     DMA2_Channel1    // DMAMUX1 channel 8 routes requests to DMA2 channel 1
#define RX_DMAMUX  DMAMUX1_Channel8
#define TX_DMA     DMA2_Channel2    // DMAMUX1 channel 9 -> DMA2 channel 2
#define TX_DMAMUX  DMAMUX1_Channel9
#define MISO_PIN   4U               // PB4
#define CS_PIN     6U               // PB6

#define FRAME_MAX   96U                          // longest command (a name is at most 20 characters)

/* Chip-select noise guard. A real master needs at least 20 us of set-up plus the bytes for each window, so
   more than CS_STORM_EDGES edges inside CS_STORM_US can only be a floating or noisy line. Then the chip-select
   interrupt is switched off and tried again CS_RETRY_MS later, so noise can never take over the processor. */
#define CS_STORM_EDGES 16U
#define CS_STORM_US    50U
#define CS_RETRY_MS    100U
#define REPORT_ROOM (LINK_XCHG_BYTES - 8U + 1U)  // a packet less "$NN:" and "#XX^", plus the '\0'

/*=============================== STATE ================================*/

static uint8_t s_rx[LINK_RX_RING];             // filled by RX DMA, read by the task
static uint16_t s_rx_tail;
static uint8_t s_tx[2][LINK_XCHG_BYTES];       // the packet being sent (s_live) and the next one
static volatile uint8_t s_live;

static uint8_t s_seq;                          // seq of the last report packet (1-99)
static int8_t s_live_idle = -1;                // the live packet is a seq-0 one: 0 keep-alive, 1 hello; -1 not
static volatile uint8_t s_unacked;             // the live packet hasn't been acknowledged (holds its seq)

static char s_frame[FRAME_MAX + 1];
static uint16_t s_frame_len;
static bool s_in_frame;

static volatile uint32_t s_windows;
static uint32_t s_storm_cycles;          // CS_STORM_US in processor cycles
static volatile uint32_t s_storm_start;  // when the current run of edges began (DWT cycles)
static volatile uint32_t s_storm_edges;
static volatile bool s_cs_muted;         // the chip-select interrupt was switched off by the noise guard
static uint32_t s_storms;
static volatile bool s_stopped; // solo mode: no master
static uint32_t s_bytes, s_frames, s_unknown, s_packets, s_acked;

static uint32_t s_stack[768];
static StaticTask_t s_tcb;

/*=============================== HARDWARE ================================*/

/** PB4 drives MISO only while this bucket is selected; otherwise it's an input and lets go of the line */
static inline void miso_drive (bool on)
{
    const uint32_t moder = GPIOB->MODER & ~(3UL << (MISO_PIN * 2U));
    GPIOB->MODER = moder | ((on ? 2UL : 0UL) << (MISO_PIN * 2U)); // 10 = alternate function (AF5, SPI1_MISO)
}

/**
 * Loads the live packet into SPI1 for the next window. SPI1 is reset first: a window shorter than
 * the packet leaves bytes in its transmit FIFO, which would otherwise go out first next time. Only
 * while not selected; runs in the chip-select interrupt, or in the task with interrupts off.
 */
static void arm_tx (void)
{
    TX_DMA->CCR &= ~DMA_CCR_EN;
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;

    LINK_SPI->CR1 = SPI_CR1_SSM | SPI_CR1_SSI;                                   // slave, mode 0, MSB first, not selected
    LINK_SPI->CR2 = (7UL << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH | SPI_CR2_RXDMAEN;   // 8-bit, receive into the ring
    DMA2->IFCR = DMA_IFCR_CGIF2;
    TX_DMA->CMAR = (uint32_t)(uintptr_t)s_tx[s_live];
    TX_DMA->CNDTR = LINK_XCHG_BYTES;
    TX_DMA->CCR |= DMA_CCR_EN;
    LINK_SPI->CR2 |= SPI_CR2_TXDMAEN;
    LINK_SPI->CR1 |= SPI_CR1_SPE; // the DMA fills the transmit FIFO now, before the master clocks
}

/** Chip select, both edges */
void EXTI9_5_IRQHandler (void)
{
    if ((EXTI->PR1 & EXTI_PR1_PIF6) == 0U) return;
    EXTI->PR1 = EXTI_PR1_PIF6;

    // noise guard: far more edges than any master could make
    const uint32_t now = DWT->CYCCNT;
    if ((uint32_t)(now - s_storm_start) > s_storm_cycles)
    {
        s_storm_start = now;
        s_storm_edges = 0;
    }
    if (++s_storm_edges > CS_STORM_EDGES)
    {
        EXTI->IMR1 &= ~EXTI_IMR1_IM6;  // stop listening; the link task tries again later
        LINK_SPI->CR1 |= SPI_CR1_SSI;  // not selected
        miso_drive(false);
        s_cs_muted = true;
        return;
    }

    if ((GPIOB->IDR & (1UL << CS_PIN)) == 0U)
    {
        LINK_SPI->CR1 &= ~SPI_CR1_SSI; // selected: shift in and out
        miso_drive(true);
    }
    else
    {
        miso_drive(false);             // the other buckets share MISO
        arm_tx();                      // ready for the next window
        s_windows++;
    }
}

static void hardware_init (void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PB4 to MISO's alternate function (AF5), but not driving it until selected
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xFUL << (MISO_PIN * 4U))) | (5UL << (MISO_PIN * 4U));
    miso_drive(false);

    // receive: SPI1 RX -> ring, circular, bytes
    RX_DMA->CCR = 0;
    RX_DMAMUX->CCR = DMA_REQUEST_SPI1_RX;
    RX_DMA->CPAR = (uint32_t)(uintptr_t)&LINK_SPI->DR;
    RX_DMA->CMAR = (uint32_t)(uintptr_t)s_rx;
    RX_DMA->CNDTR = LINK_RX_RING;
    RX_DMA->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PL_1; // peripheral to memory, 8-bit both sides
    RX_DMA->CCR |= DMA_CCR_EN;

    // transmit: packet -> SPI1 TX, one packet per window, bytes (8-bit writes, so the SPI doesn't pack two)
    TX_DMA->CCR = 0;
    TX_DMAMUX->CCR = DMA_REQUEST_SPI1_TX;
    TX_DMA->CPAR = (uint32_t)(uintptr_t)&LINK_SPI->DR;
    TX_DMA->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_PL_1;

    memset(s_tx, 0, sizeof(s_tx));
    s_live = 0;
    arm_tx();

    // chip select on PB6: pulled up, so with no master driving it (or none plugged in) it reads "not selected"
    // instead of floating; the master's push-pull output overrides the pull-up
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3UL << (CS_PIN * 2U))) | (1UL << (CS_PIN * 2U));

    // EXTI6 from port B, both edges; not unmasked until the link task starts (cs_listen), so nothing on this line
    // can ever hold up start-up
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI6) | (1UL << SYSCFG_EXTICR2_EXTI6_Pos);
    EXTI->IMR1 &= ~EXTI_IMR1_IM6;
    EXTI->RTSR1 |= EXTI_RTSR1_RT6;
    EXTI->FTSR1 |= EXTI_FTSR1_FT6;
    EXTI->PR1 = EXTI_PR1_PIF6;
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 1, 0); // above FreeRTOS's limit: never delayed by it, never calls it
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    // the noise guard times edges with the cycle counter
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    s_storm_cycles = (SystemCoreClock / 1000000U) * CS_STORM_US;
}

/** Starts (or restarts) listening to the chip select: the next window after this is received in full */
static void cs_listen (void)
{
    __disable_irq();
    if (s_stopped) // solo mode was decided meanwhile: stay off
    {
        __enable_irq();
        return;
    }
    arm_tx(); // not selected, the live packet loaded
    s_storm_edges = 0;
    s_storm_start = DWT->CYCCNT;
    s_cs_muted = false;
    EXTI->PR1 = EXTI_PR1_PIF6;
    EXTI->IMR1 |= EXTI_IMR1_IM6;
    __enable_irq();
}

/** After the noise guard has switched the chip select off, tries again every CS_RETRY_MS */
static void cs_watch (void)
{
    static bool waiting;
    static TickType_t since;
    if (!s_cs_muted)
    {
        waiting = false;
        return;
    }
    if (!waiting)
    {
        waiting = true;
        since = xTaskGetTickCount();
        s_storms++;
#if DEVELOPER_MODE
        if (s_storms == 1U) printf("\r\nMaster link: chip select (PB6) is noisy; ignoring it for %u ms at a time\n", (unsigned)CS_RETRY_MS);
#endif
    }
    else if ((TickType_t)(xTaskGetTickCount() - since) >= pdMS_TO_TICKS(CS_RETRY_MS))
    {
        waiting = false;
        cs_listen();
    }
}

/** Makes s_tx[spare] live. If no window is open it's loaded straight away, else at the window's end. */
static void publish (uint8_t spare)
{
    __disable_irq(); // FreeRTOS's own masking doesn't hold off the priority-1 chip-select interrupt
    s_live = spare;
    if ((GPIOB->IDR & (1UL << CS_PIN)) != 0U) arm_tx();
    __enable_irq();
}

/*=============================== RECEIVING ================================*/

static void feed (char c)
{
    s_bytes++;
    if (c == '%' && !s_in_frame)
    {
        s_in_frame = true;
        s_frame_len = 0;
    }
    else if (s_in_frame)
    {
        if (c == '%')
        {
            s_frame[s_frame_len] = '\0';
            if (cmd_exec(s_frame))
            {
                s_in_frame = false;
                s_frames++;
            }
            else
            {
                // not a command: a byte was lost, so this % opens the next frame
                if (s_frame_len) s_unknown++;
                s_frame_len = 0;
            }
        }
        else if (s_frame_len >= FRAME_MAX)
        {
            s_in_frame = false; // too long: wait for the next opening %
            s_frame_len = 0;
        }
        else
        {
            s_frame[s_frame_len++] = c;
        }
    }
    // else: between frames (the master's 0xFF padding), ignored
}

static void drain_rx (void)
{
    const uint16_t head = (uint16_t)((LINK_RX_RING - RX_DMA->CNDTR) & (LINK_RX_RING - 1U));
    while (s_rx_tail != head)
    {
        feed((char)s_rx[s_rx_tail]);
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) & (LINK_RX_RING - 1U));
    }
}

void master_link_ack (uint8_t seq)
{
    if (seq != 0U && seq == s_unacked)
    {
        s_unacked = 0;
        s_acked++;
    }
}

/*=============================== SENDING ================================*/

/** Looks at every channel's controls (copied under their locks) for anything the user changed */
static void scan_panel (void)
{
    int8_t selected = -1;
    for (uint8_t ch = 0; ch < CHANNELS; ch++)
    {
        channel_controls copy;
        xSemaphoreTake(channel_control_mutexes[ch], portMAX_DELAY);
        copy = channel_control_vals[ch];
        xSemaphoreGive(channel_control_mutexes[ch]);
        report_scan(ch, &copy);
        if (copy.sel && selected < 0) selected = (int8_t)ch;
    }
    report_scan_select(selected);
}

/** "$<seq>:<reports>#<xx>^", zero padded */
static void compose (uint8_t *packet, uint8_t seq, const char *reports, uint16_t len)
{
    memset(packet, 0, LINK_XCHG_BYTES);
    char *p = (char *)packet;
    uint16_t n = 0;
    p[n++] = '$';
    if (seq >= 10U) p[n++] = (char)('0' + seq / 10U);
    p[n++] = (char)('0' + seq % 10U);
    p[n++] = ':';
    memcpy(&p[n], reports, len);
    n = (uint16_t)(n + len);
    uint8_t x = 0;
    for (uint16_t i = 1; i < n; i++) x ^= (uint8_t)p[i]; // after '$', up to '#'
    static const char HEX[] = "0123456789ABCDEF";
    p[n++] = '#';
    p[n++] = HEX[x >> 4];
    p[n++] = HEX[x & 0xFU];
    p[n++] = '^';
}

/**
 * Every window carries a valid packet, so the master can always tell this bucket is there:
 *   - panel changes, seq 1-99, sent until the master acknowledges them (only once the master has
 *     given us our channels: they carry console channel numbers);
 *   - otherwise a seq-0 packet the master needn't acknowledge: "$0:hi#xx^" (hello) until the master
 *     has given us our channels, "$0:#xx^" (keep-alive) after.
 */
static void send_next (void)
{
    if (s_unacked != 0U) return; // still sending the last one until the master acknowledges it
    const uint8_t spare = (uint8_t)(s_live ^ 1U);

    if (console_state_from_master() && report_pending())
    {
        char reports[REPORT_ROOM];
        const uint16_t len = report_build(reports, sizeof(reports), console_channel_index(0));
        if (len > 0U)
        {
            s_seq = (uint8_t)(s_seq % 99U + 1U); // 1-99, never 0
            compose(s_tx[spare], s_seq, reports, len);
            s_unacked = s_seq;
            s_live_idle = -1;
            s_packets++;
            publish(spare);
            return;
        }
    }
    const int8_t idle = console_state_from_master() ? 0 : 1;
    if (s_live_idle != idle)
    {
        compose(s_tx[spare], 0, idle ? "hi" : "", idle ? 2U : 0U);
        s_live_idle = idle;
        publish(spare);
    }
}

/*=============================== TASK ================================*/

static void link_task (void *argument)
{
    (void)argument;
    if (!s_stopped) cs_listen(); // only now, with everything else started
    TickType_t last_scan = xTaskGetTickCount();
    for (;;)
    {
        if (s_stopped) osThreadExit(); // solo mode: nothing more to do here
        cs_watch();
        drain_rx(); // commands from the master: values, our channel numbers, acknowledgements
        if ((TickType_t)(xTaskGetTickCount() - last_scan) >= pdMS_TO_TICKS(LINK_SCAN_MS))
        {
            last_scan = xTaskGetTickCount();
            scan_panel();
        }
        send_next();
        osDelay(1);
    }
}

void master_link_init (void)
{
    hardware_init();
    static const osThreadAttr_t attr = {
        .name = "master_link",
        .priority = osPriorityNormal, // above the I2C tasks: commands and packets every millisecond
        .stack_mem = s_stack,
        .stack_size = sizeof(s_stack),
        .cb_mem = &s_tcb,
        .cb_size = sizeof(s_tcb),
    };
    osThreadNew(link_task, NULL, &attr);
#if DEVELOPER_MODE
    printf("\r\nMaster link: listening on SPI1 (chip select PB6); channel numbers come from the master\n");
#endif
}

void master_link_stop (void)
{
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    EXTI->IMR1 &= ~EXTI_IMR1_IM6;
    __disable_irq();
    miso_drive(false); // never drive the shared MISO line
    LINK_SPI->CR1 &= ~SPI_CR1_SPE;
    LINK_SPI->CR2 = 0;
    TX_DMA->CCR &= ~DMA_CCR_EN;
    RX_DMA->CCR &= ~DMA_CCR_EN;
    s_stopped = true;
    __enable_irq();
}

void master_link_counts (master_link_stats_t *out)
{
    if (out == NULL) return;
    out->windows = s_windows;
    out->bytes = s_bytes;
    out->frames = s_frames;
    out->unknown = s_unknown;
    out->packets = s_packets;
    out->acked = s_acked;
}
