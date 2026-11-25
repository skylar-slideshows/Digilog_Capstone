/**
  **********************************************************************************
  * LOG UART - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file hardware_drivers/log_uart.c
  * @brief printf() goes out on the log UART (USART2, 921600 baud, the ST-Link's virtual COM
  *        port), as it does on the bucket, so the copied drivers' developer-mode messages show
  *        up. A character the UART can't take within LOG_UART_TIMEOUT_MS (another task holding
  *        it, say daw_control's log lines) is dropped rather than waited for.
  **********************************************************************************
*/

#include "CONFIG.h"
#include "main.h"

extern UART_HandleTypeDef LOG_UART_HANDLE; // from main.c

int __io_putchar (int ch)
{
    uint8_t c = (uint8_t)ch;
    (void)HAL_UART_Transmit(&LOG_UART_HANDLE, &c, 1, LOG_UART_TIMEOUT_MS);
    return ch;
}
