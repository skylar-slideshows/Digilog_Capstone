/*
 * Levetop's LT768x library (LT768_Lib V1.3), ported to the Digilog master controller.
 *
 * if_port.c: the 16-bit 8080 bus to the LT7683 on the G474's FMC. See if_port.h.
 */

#include "if_port.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

extern SRAM_HandleTypeDef hsram1; // FMC bank 1, from MX_FMC_Init

/*
 * Timings, in HCLK cycles of 5.9 ns (170 MHz). Levetop's STM32F103 example runs reads and writes
 * at about 2 + 2 cycles of 72 MHz (~55 ns); these are a little slower than that for writes and
 * about twice it for reads (status polls), and the LT7683 can still stretch any access with its
 * WAIT line (asynchronous wait is switched on in MX_FMC_Init's user code, and kept here).
 */
void Parallel_Init(void)
{
    FMC_NORSRAM_TimingTypeDef read = {0};
    read.AddressSetupTime      = 4;  // 24 ns
    read.AddressHoldTime       = 1;  // (unused in mode A)
    read.DataSetupTime         = 16; // 94 ns
    read.DataHoldTime          = 0;
    read.BusTurnAroundDuration = 2;  // 12 ns between accesses
    read.CLKDivision           = 2;  // (synchronous only)
    read.DataLatency           = 2;  // (synchronous only)
    read.AccessMode            = FMC_ACCESS_MODE_A;

    FMC_NORSRAM_TimingTypeDef write = read;
    write.DataSetupTime = 8; // 47 ns

    hsram1.Init.ExtendedMode     = FMC_EXTENDED_MODE_ENABLE; // separate read and write timings
    hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_ENABLE;
    (void)HAL_SRAM_Init(&hsram1, &read, &write);
}

void LCD_CmdWrite(u8 cmd)
{
    *(vu16 *)LCD_BASE0 = cmd;
}

void LCD_DataWrite(u8 data)
{
    *(vu16 *)LCD_BASE1 = data;
}

void LCD_DataWrite_Pixel(u16 data)
{
    *(vu16 *)LCD_BASE1 = data;
}

u8 LCD_StatusRead(void)
{
    return (u8)(*(vu16 *)LCD_BASE0);
}

u16 LCD_DataRead(void)
{
    return *(vu16 *)LCD_BASE1;
}

void Delay_us(u16 time)
{
    // the cycle counter (DWT), switched on by i2c_init(); switched on here too in case
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles = (SystemCoreClock / 1000000U) * time;
    while ((DWT->CYCCNT - start) < cycles) {}
}

void Delay_ms(u16 time)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        vTaskDelay(pdMS_TO_TICKS(time)); // lets the other tasks run meanwhile
    }
    else
    {
        while (time--) Delay_us(1000);
    }
}
