/**
  **********************************************************************************
  * DISPLAYS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/displays.c
  * @brief Runs the four ST7789 screens over the 8-bit FMC bus, using LibDriver's
  *        st7789 driver for the drawing.
  *
  * All four panels share the data bus, the read/write strobes, A0 and reset; only their
  * chip selects differ. So one driver handle serves all four: pick a panel by pulling its
  * chip select low, call the driver, release it.
  *
  * The FMC does the bus timing. A byte written to DISP_CMD goes out with A0 low (a command);
  * a byte written to DISP_DATA goes out with A0 high (a parameter or pixel data).
  **********************************************************************************
*/

#include "FreeRTOS_apps/displays.h"
#include "CONFIG.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hardware_drivers/driver_st7789.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DISPLAY_COUNT     4
#define DISPLAY_SIZE_PX   240
#define BACKLIGHT_PERCENT 75 // 0-100

#ifndef DISP_FMC_BASE
#define DISP_FMC_BASE 0x60000000U // FMC bank 1 (NE1)
#endif
#define DISP_CMD  ((volatile uint8_t *)(DISP_FMC_BASE))      // A0 low:  command
#define DISP_DATA ((volatile uint8_t *)(DISP_FMC_BASE + 1U)) // A0 high: data

#define COLOR_BLACK 0x0000U // RGB565
#define COLOR_WHITE 0xFFFFU
#define COLOR_RED   0xF800U
#define COLOR_GREEN 0x07E0U
#define COLOR_BLUE  0x001FU

extern TIM_HandleTypeDef htim1; // from main.c; channel 3 is DISP_PWM

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} disp_cs_t;

static const disp_cs_t DISP_CS[DISPLAY_COUNT] = {
    {DISP_CS0_GPIO_Port, DISP_CS0_Pin},
    {DISP_CS1_GPIO_Port, DISP_CS1_Pin},
    {DISP_CS2_GPIO_Port, DISP_CS2_Pin},
    {DISP_CS3_GPIO_Port, DISP_CS3_Pin},
};

static st7789_handle_t s_st7789;               // one handle for all four panels
static volatile uint8_t *s_target = DISP_CMD;  // where the next bytes go; set by the command/data hook
static SemaphoreHandle_t s_lock;               // one bus and one handle: one drawing operation at a time


/*=============================== DRIVER HOOKS ================================*/

static uint8_t hook_nothing (void)
{
    return 0; // the FMC and the GPIO pins are already set up by CubeMX
}

static uint8_t hook_command_or_data (uint8_t value)
{
    s_target = value ? DISP_DATA : DISP_CMD; // the driver passes 0 for a command, 1 for data
    return 0;
}

static uint8_t hook_write (uint8_t *buf, uint16_t len)
{
    volatile uint8_t *const reg = s_target;
    while (len--)
    {
        *reg = *buf++;
    }
    return 0;
}

static uint8_t hook_reset (uint8_t value)
{
    HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}

static void hook_delay_ms (uint32_t ms)
{
    // Counts CPU cycles rather than using HAL_Delay(). Once anything creates a FreeRTOS object
    // before the scheduler starts, FreeRTOS masks the interrupt behind HAL_GetTick() until the
    // scheduler runs, so HAL_Delay() would never return during startup.
    const uint32_t cycles_per_ms = SystemCoreClock / 1000U;
    while (ms--)
    {
        const uint32_t t0 = DWT->CYCCNT;
        while ((DWT->CYCCNT - t0) < cycles_per_ms)
        {
        }
    }
}

static void hook_debug_print (const char *const fmt, ...)
{
#if DEVELOPER_MODE
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}


/*=============================== PANEL SELECTION ================================*/

static void select_display (uint8_t display)
{
    HAL_GPIO_WritePin(DISP_CS[display].port, DISP_CS[display].pin, GPIO_PIN_RESET);
}

static void release_display (uint8_t display)
{
    // The FMC queues writes, so the CPU can get here before the last byte has gone out. A read
    // can't complete until every queued write has, so do one before letting go of the chip
    // select; otherwise the final byte could be cut off.
    (void)*DISP_DATA;
    HAL_GPIO_WritePin(DISP_CS[display].port, DISP_CS[display].pin, GPIO_PIN_SET);
}

static void lock (void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock (void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) xSemaphoreGive(s_lock);
}


/*=============================== PUBLIC FUNCTIONS ================================*/

uint8_t displays_init (void)
{
    // CubeMX starts every chip select low, which selects all four panels at once
    for (uint8_t d = 0; d < DISPLAY_COUNT; d++)
    {
        HAL_GPIO_WritePin(DISP_CS[d].port, DISP_CS[d].pin, GPIO_PIN_SET);
    }

    // the delay hook counts CPU cycles
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    s_lock = xSemaphoreCreateMutex();

    // Backlight: TIM1 channel 3, on while high. TIM1 also runs the LED brightness PWM and
    // led_init() changes its period, so use the period the timer is actually running at.
    const uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (period * BACKLIGHT_PERCENT) / 100U);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    DRIVER_ST7789_LINK_INIT(&s_st7789, st7789_handle_t);
    DRIVER_ST7789_LINK_SPI_INIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_SPI_DEINIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_SPI_WRITE_COMMAND(&s_st7789, hook_write);
    DRIVER_ST7789_LINK_COMMAND_DATA_GPIO_INIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_COMMAND_DATA_GPIO_DEINIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_COMMAND_DATA_GPIO_WRITE(&s_st7789, hook_command_or_data);
    DRIVER_ST7789_LINK_RESET_GPIO_INIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_RESET_GPIO_DEINIT(&s_st7789, hook_nothing);
    DRIVER_ST7789_LINK_RESET_GPIO_WRITE(&s_st7789, hook_reset);
    DRIVER_ST7789_LINK_DELAY_MS(&s_st7789, hook_delay_ms);
    DRIVER_ST7789_LINK_DEBUG_PRINT(&s_st7789, hook_debug_print);

    // pulses the shared reset line, resetting all four panels at once, and waits for them
    if (st7789_init(&s_st7789) != 0) return 1;

    // the screen size, for the drawing functions' bounds checks; nothing is sent to the panels
    st7789_set_column(&s_st7789, DISPLAY_SIZE_PX);
    st7789_set_row(&s_st7789, DISPLAY_SIZE_PX);

    // st7789_init() only resets the panels. This is the setup that actually turns them on.
    uint8_t res = 0;
    for (uint8_t d = 0; d < DISPLAY_COUNT; d++)
    {
        select_display(d);
        res |= st7789_sleep_out(&s_st7789); // waits the 200 ms the panel needs, inside the driver
        res |= st7789_set_interface_pixel_format(&s_st7789,
                                                 ST7789_RGB_INTERFACE_COLOR_FORMAT_65K,
                                                 ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT); // RGB565
        res |= st7789_set_memory_data_access_control(&s_st7789, ST7789_ORDER_COLOR_RGB); // red and blue swapped? use ST7789_ORDER_COLOR_BGR
        res |= st7789_display_inversion_on(&s_st7789); // IPS panels need this; black shows as white? use st7789_display_inversion_off
        res |= st7789_normal_display_mode_on(&s_st7789);
        res |= st7789_display_on(&s_st7789);
        release_display(d);
    }
    return res ? 1 : 0;
}


static void draw_centered (uint16_t y, const char *text, uint32_t color)
{
    const uint16_t len = (uint16_t)strlen(text);
    const uint16_t x = (DISPLAY_SIZE_PX - len * (ST7789_FONT_24 / 2)) / 2;
    st7789_write_string(&s_st7789, x, y, (char *)text, len, color, ST7789_FONT_24);
}

void displays_test_pattern (void)
{
    static const char *const names[DISPLAY_COUNT] = {"DISPLAY 0", "DISPLAY 1", "DISPLAY 2", "DISPLAY 3"};

    lock();
    for (uint8_t d = 0; d < DISPLAY_COUNT; d++)
    {
        select_display(d);
        st7789_clear(&s_st7789);              // black
        draw_centered(18, names[d], COLOR_WHITE); // each screen should show its own number

        // each bar should match its label
        st7789_fill_rect(&s_st7789, 0, 60, DISPLAY_SIZE_PX - 1, 119, COLOR_RED);
        st7789_fill_rect(&s_st7789, 0, 120, DISPLAY_SIZE_PX - 1, 179, COLOR_GREEN);
        st7789_fill_rect(&s_st7789, 0, 180, DISPLAY_SIZE_PX - 1, 239, COLOR_BLUE);
        draw_centered(78, "RED", COLOR_WHITE);
        draw_centered(138, "GREEN", COLOR_WHITE);
        draw_centered(198, "BLUE", COLOR_WHITE);
        release_display(d);
    }
    unlock();
}


uint8_t displays_draw_text (uint8_t display, uint16_t x, uint16_t y, uint8_t size, const char *text)
{
    if (display >= DISPLAY_COUNT || text == NULL) return 1;
    if (size != ST7789_FONT_12 && size != ST7789_FONT_16 && size != ST7789_FONT_24) return 1;

    // the driver jumps to the next line, or back to the top-left corner, near the edges
    if (x + size / 2 >= DISPLAY_SIZE_PX || y + size >= DISPLAY_SIZE_PX) return 1;

    const uint16_t len = (uint16_t)strlen(text);
    if (len == 0) return 0;

    // The black box behind the text makes new text replace old instead of drawing on top
    // of it. It covers one line: text too long for the line wraps without a box behind it.
    uint32_t right = x + (uint32_t)len * (size / 2) - 1U;
    if (right > DISPLAY_SIZE_PX - 1U) right = DISPLAY_SIZE_PX - 1U;

    lock();
    select_display(display);
    uint8_t res = st7789_fill_rect(&s_st7789, x, y, (uint16_t)right, y + size - 1U, COLOR_BLACK);
    res |= st7789_write_string(&s_st7789, x, y, (char *)text, len, COLOR_WHITE, (st7789_font_t)size);
    release_display(display);
    unlock();

    return res ? 1 : 0;
}