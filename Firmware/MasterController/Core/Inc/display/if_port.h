/*
 * Levetop's LT768x library (LT768_Lib V1.3), ported to the Digilog master controller.
 *
 * if_port.h: the bus to the LT7683. Levetop's original picks between STM32F103 FSMC (8 or 16
 * bit), SPI and I2C; the master has only the 16-bit 8080 bus on the G474's FMC, so that is the
 * only one here.
 *
 *   FMC bank 1, NE1 (PD7), 16-bit data, NOE PD4, NWE PD5, NWAIT PD6 (the LT7683's WAIT).
 *   The LT7683's RS pin is FMC_A0 (PF10). On a 16-bit bus FMC_A0 is address bit 1, so:
 *     0x60000000  RS low:  command write / status read
 *     0x60000002  RS high: data write / data read
 */

#ifndef _if_port_h
#define _if_port_h

#include <stdint.h>

// the types Levetop's code uses (from the F103's sys.h)
typedef uint8_t           u8;
typedef uint16_t          u16;
typedef uint32_t          u32;
typedef volatile uint8_t  vu8;
typedef volatile uint16_t vu16;
typedef volatile uint32_t vu32;

#define STM32_FSMC_8  0 // kept so the library's #if lines still read as Levetop wrote them
#define STM32_FSMC_16 1
#define STM32_SPI     0
#define STM32_IIC     0

#define LCD_BASE0 ((u32)0x60000000) // RS low
#define LCD_BASE1 ((u32)0x60000002) // RS high

/** FMC read/write timings for the LT7683 (CubeMX set the pins and a very slow timing). */
void Parallel_Init(void);

void LCD_CmdWrite(u8 cmd);
void LCD_DataWrite(u8 data);
void LCD_DataWrite_Pixel(u16 data);
u8   LCD_StatusRead(void);
u16  LCD_DataRead(void);

void Delay_us(u16 time); // busy-wait
void Delay_ms(u16 time); // sleeps (FreeRTOS) once the scheduler runs

#endif
