/**
  **********************************************************************************
  * DIAGNOSTICS - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/diagnostics.c
  * @brief See diagnostics.h. Everything goes out on the log UART (USART2, 921600 baud),
  *        the ST-Link's virtual serial port on a Nucleo.
  *
  * The USB watcher only reads TinyUSB's state (inited, talking to a computer, configured,
  * suspended); it never calls into the stack itself.
  *
  * The fault reports write the UART's registers directly, so they work with interrupts off,
  * inside a HardFault, and before the UART or the scheduler has been started.
  **********************************************************************************
*/

#include "FreeRTOS_apps/diagnostics.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "tusb.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef LOG_UART_HANDLE;

#define WATCH_MS        250 // how often the USB watcher looks
#define STALL_REPORT_MS 5000


/*=============================== OUTPUT ================================*/

void diag_log (const char *text)
{
    char line[160];
    const int n = snprintf(line, sizeof(line), "%s\r\n", text);
    if (n > 0) HAL_UART_Transmit(&LOG_UART_HANDLE, (uint8_t *)line, (uint16_t)strlen(line), LOG_UART_TIMEOUT_MS);
}


// Sets up USART2 if Error_Handler came before MX_USART2_UART_Init (it runs last in main)
static void raw_uart_ready (void)
{
    if (LOG_UART_HANDLE.gState != HAL_UART_STATE_RESET) return;

    LOG_UART_HANDLE.Instance                    = USART2;
    LOG_UART_HANDLE.Init.BaudRate               = 921600;
    LOG_UART_HANDLE.Init.WordLength             = UART_WORDLENGTH_8B;
    LOG_UART_HANDLE.Init.StopBits               = UART_STOPBITS_1;
    LOG_UART_HANDLE.Init.Parity                 = UART_PARITY_NONE;
    LOG_UART_HANDLE.Init.Mode                   = UART_MODE_TX_RX;
    LOG_UART_HANDLE.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    LOG_UART_HANDLE.Init.OverSampling           = UART_OVERSAMPLING_16;
    LOG_UART_HANDLE.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    LOG_UART_HANDLE.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    LOG_UART_HANDLE.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    (void)HAL_UART_Init(&LOG_UART_HANDLE);
}


static void raw_write (const char *s)
{
    USART_TypeDef *u = LOG_UART_HANDLE.Instance;
    if (u == NULL) return;
    while (*s)
    {
        while (!(u->ISR & USART_ISR_TXE_TXFNF)) {}
        u->TDR = (uint8_t)*s++;
    }
    while (!(u->ISR & USART_ISR_TC)) {}
}


static void hex32 (char *out, uint32_t v) // "0x0800ABCD"
{
    static const char digits[] = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 8; i++) out[2 + i] = digits[(v >> (28 - 4 * i)) & 0xF];
    out[10] = '\0';
}


static void decimal (char *out, uint32_t v) // up to 10 digits
{
    char tmp[11];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10U); v /= 10U; } while (v != 0 && n < 10);
    while (n) *out++ = tmp[--n];
    *out = '\0';
}


// Repeats the message (the pieces, up to a NULL) about once a second, forever. No formatting
// and no heap: it may be running inside a HardFault on a nearly full stack.
static void __attribute__((noreturn)) halt_reporting (const char *first, ...)
{
    __disable_irq();
    raw_uart_ready();
    for (;;)
    {
        raw_write("\r\n!! ");
        va_list parts;
        va_start(parts, first);
        for (const char *p = first; p != NULL; p = va_arg(parts, const char *)) raw_write(p);
        va_end(parts);
        raw_write("\r\n");
        for (volatile uint32_t i = 0; i < SystemCoreClock / 16U; i++) {} // about a second
    }
}


/*=============================== FAULT REPORTS ================================*/

void diag_error_handler (uint32_t caller)
{
    char where[11];
    hex32(where, caller);
    halt_reporting("Error_Handler called from ", where,
                   " - find the line with: arm-none-eabi-addr2line -e <your .elf> <that address>", NULL);
}


void diag_hardfault (uint32_t exc_return)
{
    // Bit 2 of the exception return value: the fault happened in a task (process stack)
    if (exc_return & 0x4U)
    {
        const uint32_t *frame = (const uint32_t *)__get_PSP();
        char where[11];
        hex32(where, frame[6]); // the stacked program counter
        const char *task = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) ? pcTaskGetName(NULL) : "?";
        halt_reporting("HardFault in task '", task, "' at ", where,
                       " - look it up with arm-none-eabi-addr2line -e <your .elf>", NULL);
    }
    halt_reporting("HardFault in an interrupt handler or before the scheduler started", NULL);
}


void diag_assert (const char *file, int line)
{
    const char *base = strrchr(file, '/');
    base = (base != NULL) ? base + 1 : file;
    char number[11];
    decimal(number, (uint32_t)line);
    halt_reporting("FreeRTOS assert failed: ", base, " line ", number, NULL);
}


/*=============================== START-UP ================================*/

void diag_boot (void)
{
    const uint32_t csr = RCC->CSR;
    const char *cause = (csr & RCC_CSR_IWDGRSTF) ? "independent watchdog"
                      : (csr & RCC_CSR_WWDGRSTF) ? "window watchdog"
                      : (csr & RCC_CSR_SFTRSTF)  ? "software reset (e.g. the debugger, or NVIC_SystemReset)"
                      : (csr & RCC_CSR_LPWRRSTF) ? "low-power reset"
                      : (csr & RCC_CSR_OBLRSTF)  ? "option byte load"
                      : (csr & RCC_CSR_BORRSTF)  ? "power on (or a brown-out)"
                      : (csr & RCC_CSR_PINRSTF)  ? "reset pin (button or ST-Link)"
                                                 : "unknown";
    RCC->CSR |= RCC_CSR_RMVF; // so the next start reports its own cause

    char line[160];
    snprintf(line, sizeof(line), "\r\n== master controller started, reset by: %s (diagnostics.c built %s %s)",
             cause, __DATE__, __TIME__);
    diag_log(line);
}


/*=============================== USB WATCHER ================================*/

static void watch_task (void *argument)
{
    (void)argument;
    char line[160];

    snprintf(line, sizeof(line), "diag: scheduler running, %u bytes of FreeRTOS heap free",
             (unsigned)xPortGetFreeHeapSize());
    diag_log(line);

    bool inited = false, connected = false, mounted = false, suspended = false;
    uint32_t stuck_ms = 0;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(WATCH_MS));

        if (!inited && tud_inited())
        {
            inited = true;
            diag_log("usb: TinyUSB started and the D+ pull-up is on: the computer should notice the device now");
        }
        if (inited && tud_connected() != connected)
        {
            connected = !connected;
            diag_log(connected ? "usb: the computer reset the bus and is sending requests"
                               : "usb: the computer stopped talking (unplugged, or a bus reset)");
            stuck_ms = 0;
        }
        if (inited && tud_mounted() != mounted)
        {
            mounted = !mounted;
            diag_log(mounted ? "usb: configured by the computer - MIDI and the serial port should appear now"
                             : "usb: no longer configured");
            stuck_ms = 0;
        }
        if (inited && tud_suspended() != suspended)
        {
            suspended = !suspended;
            diag_log(suspended ? "usb: suspended by the computer" : "usb: resumed");
        }

        if (mounted) continue;
        stuck_ms += WATCH_MS;
        if (stuck_ms % STALL_REPORT_MS != 0) continue;

        if (!inited)
        {
            snprintf(line, sizeof(line), "usb: %lu s and TinyUSB still hasn't started - the USB task isn't running",
                     (unsigned long)(stuck_ms / 1000U));
        }
        else if (!connected)
        {
            snprintf(line, sizeof(line), "usb: %lu s and no request from the computer - check D+ (PA12), D- (PA11) "
                     "and GND to the connector, the cable, and the computer's port", (unsigned long)(stuck_ms / 1000U));
        }
        else
        {
            snprintf(line, sizeof(line), "usb: %lu s talking but not configured - the computer rejected the device "
                     "during enumeration", (unsigned long)(stuck_ms / 1000U));
        }
        diag_log(line);
    }
}


void diag_start (void)
{
    static uint32_t     stack[512];
    static StaticTask_t tcb;
    static const osThreadAttr_t attr = {
        .name = "diag", .priority = osPriorityLow,
        .stack_mem = stack, .stack_size = sizeof(stack),
        .cb_mem = &tcb,     .cb_size = sizeof(tcb),
    };
    if (osThreadNew(watch_task, NULL, &attr) == NULL) diag_log("diag: couldn't start the USB watcher");
}
