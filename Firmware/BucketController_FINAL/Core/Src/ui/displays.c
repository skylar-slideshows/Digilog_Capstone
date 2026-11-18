/**
  **********************************************************************************
  * DISPLAYS - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file FreeRTOS_apps/displays.c
  * @brief Runs the four ST7789 screens over the 8-bit FMC bus, using LibDriver's
  *        st7789 driver for the drawing. Pictures come from st7789_imagegen.py.
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

#define DISPLAY_SIZE_PX   240
#define BACKLIGHT_PERCENT 75 // 0-100: screen brightness (TIM1 channel 3 duty; the PWM frequency is BRIGHTNESS_PWM_HZ in CONFIG.h)

// The screens are mounted upside down relative to the panel's own orientation, so everything is turned
// 180 degrees. Turned, the 240 visible rows are rows 80-319 of the panel's 320-row memory, so every row
// address gets ROW_OFFSET added. Set DISPLAY_FLIP_180 to 0 to undo.
#define DISPLAY_FLIP_180 1
#if DISPLAY_FLIP_180
#define PANEL_ORIENTATION (ST7789_ORDER_PAGE_BOTTOM_TO_TOP | ST7789_ORDER_COLUMN_RIGHT_TO_LEFT | ST7789_ORDER_COLOR_RGB)
#define ROW_OFFSET 80U
#else
#define PANEL_ORIENTATION ST7789_ORDER_COLOR_RGB
#define ROW_OFFSET 0U
#endif

#ifndef DISP_FMC_BASE
#define DISP_FMC_BASE 0x60000000U // FMC bank 1 (NE1)
#endif
#define DISP_CMD  ((volatile uint8_t *)(DISP_FMC_BASE))      // A0 low:  command
#define DISP_DATA ((volatile uint8_t *)(DISP_FMC_BASE + 1U)) // A0 high: data

// Colours are written as 8-bit red, green, blue and converted to the screens' colour depth
#if DISPLAY_COLOR_BITS == 16
#define PIXEL_FORMAT ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT
#define RGB(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)) // RGB565
#define IMAGE_BYTES(pixels) ((uint32_t)(pixels) * 2U)
#elif DISPLAY_COLOR_BITS == 12
#define PIXEL_FORMAT ST7789_CONTROL_INTERFACE_COLOR_FORMAT_12_BIT
#define RGB(r, g, b) ((((r) >> 4) << 8) | (((g) >> 4) << 4) | ((b) >> 4))   // RGB444
#define IMAGE_BYTES(pixels) (((uint32_t)(pixels) * 3U + 1U) / 2U)            // 3 bytes per 2 pixels
#else
#error "DISPLAY_COLOR_BITS must be 16 or 12"
#endif


// Regions go out in pieces of this many bytes: a multiple of 6, so a piece always ends on a whole
// pixel at both depths (12-bit pixels come in pairs of 3 bytes). Sized for two 240-pixel rows at 12 bits.
#define REGION_CHUNK 720U

// The driver sends at most 65,535 bytes per call. This is the largest size that is always a whole
// number of pixels at both depths (2 bytes each, or 3 bytes per 2), so each call ends on a pixel.
#define IMAGE_CHUNK 65532U

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
static uint32_t s_row[DISPLAY_SIZE_PX];         // displays_draw_region(): one row in 0xRRGGBB
static uint8_t s_chunk[REGION_CHUNK + 2];       // displays_draw_region(): bytes waiting to go out


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
                                                 PIXEL_FORMAT); // DISPLAY_COLOR_BITS
        res |= st7789_set_memory_data_access_control(&s_st7789, PANEL_ORIENTATION); // red and blue swapped? use ST7789_ORDER_COLOR_BGR
        res |= st7789_display_inversion_on(&s_st7789); // IPS panels need this; black shows as white? use st7789_display_inversion_off
        res |= st7789_normal_display_mode_on(&s_st7789);
        res |= st7789_display_on(&s_st7789);
        release_display(d);
    }
    return res ? 1 : 0;
}


static ui_text_t centered (int16_t y, const char *text)
{
    const int16_t x = (int16_t)((DISPLAY_SIZE_PX - ui_text_width(text, 24)) / 2);
    return (ui_text_t){.x = x, .y = y, .size = 24, .rgb = UI_RGB(255, 255, 255), .text = text};
}

void displays_test_pattern (void)
{
    static const char *const names[DISPLAY_COUNT] = {"DISPLAY 0", "DISPLAY 1", "DISPLAY 2", "DISPLAY 3"};

    for (uint8_t d = 0; d < DISPLAY_COUNT; d++)
    {
        // each screen should show its own number; each bar should match its label
        const ui_rect_t bars[3] = {
            {0, 60, DISPLAY_SIZE_PX - 1, 119, UI_RGB(255, 0, 0)},
            {0, 120, DISPLAY_SIZE_PX - 1, 179, UI_RGB(0, 255, 0)},
            {0, 180, DISPLAY_SIZE_PX - 1, 239, UI_RGB(0, 0, 255)},
        };
        const ui_text_t labels[4] = {centered(18, names[d]), centered(78, "RED"), centered(138, "GREEN"), centered(198, "BLUE")};
        const ui_region_t screen = {
            .x0 = 0, .y0 = 0, .x1 = DISPLAY_SIZE_PX - 1, .y1 = DISPLAY_SIZE_PX - 1,
            .background = UI_RGB(0, 0, 0),
            .rects = bars, .rect_count = 3,
            .texts = labels, .text_count = 4,
        };
        displays_draw_region(d, &screen);
    }
}


uint8_t displays_draw_text (uint8_t display, uint16_t x, uint16_t y, uint8_t size, const char *text)
{
    if (display >= DISPLAY_COUNT || text == NULL) return 1;
    if (size != 12 && size != 16 && size != 24) return 1;
    if (x + size / 2 >= DISPLAY_SIZE_PX || y + size > DISPLAY_SIZE_PX) return 1;

    const uint16_t len = (uint16_t)strlen(text);
    if (len == 0) return 0;

    // The black box behind the text makes new text replace old instead of drawing on top of it.
    // Text past the right edge is cut off.
    uint32_t right = x + (uint32_t)len * (size / 2U) - 1U;
    if (right > DISPLAY_SIZE_PX - 1U) right = DISPLAY_SIZE_PX - 1U;

    const ui_text_t line = {.x = (int16_t)x, .y = (int16_t)y, .size = size, .rgb = UI_RGB(255, 255, 255), .text = text};
    const ui_region_t box = {
        .x0 = (int16_t)x, .y0 = (int16_t)y, .x1 = (int16_t)right, .y1 = (int16_t)(y + size - 1U),
        .background = UI_RGB(0, 0, 0),
        .texts = &line, .text_count = 1,
    };
    return displays_draw_region(display, &box);
}


uint8_t displays_draw_image (uint8_t display, uint16_t x, uint16_t y, const display_image_t *image)
{
    if (display >= DISPLAY_COUNT || image == NULL || image->pixels == NULL) return 1;
    if (image->bits != DISPLAY_COLOR_BITS) return 1; // made for the other colour depth
    if (image->width == 0 || image->height == 0) return 1;
    if ((uint32_t)x + image->width > DISPLAY_SIZE_PX || (uint32_t)y + image->height > DISPLAY_SIZE_PX) return 1;

    // The bytes are already in the screens' format, so they go straight onto the bus. This is
    // deliberately not st7789_draw_picture_12bits(), which garbles every picture: its loop writes
    // its buffer at the wrong index and treats each pixel as two.
    const uint8_t *next = image->pixels;
    uint32_t remaining = IMAGE_BYTES((uint32_t)image->width * image->height);

    lock();
    select_display(display);
    uint8_t res = st7789_set_column_address(&s_st7789, x, x + image->width - 1U);
    res |= st7789_set_row_address(&s_st7789, (uint16_t)(y + ROW_OFFSET), (uint16_t)(y + image->height - 1U + ROW_OFFSET));
    for (uint8_t first = 1; remaining > 0 && res == 0; first = 0)
    {
        const uint16_t n = (uint16_t)(remaining > IMAGE_CHUNK ? IMAGE_CHUNK : remaining);
        res |= first ? st7789_memory_write(&s_st7789, (uint8_t *)next, n)
                     : st7789_memory_continue_write(&s_st7789, (uint8_t *)next, n);
        next += n;
        remaining -= n;
    }
    release_display(display);
    unlock();

    return res ? 1 : 0;
}


uint8_t displays_clear (uint8_t display)
{
    const ui_region_t black = {.x0 = 0, .y0 = 0, .x1 = DISPLAY_SIZE_PX - 1, .y1 = DISPLAY_SIZE_PX - 1, .background = UI_RGB(0, 0, 0)};
    return displays_draw_region(display, &black);
}


// 8-bit colour channel to the screens' 4, 5 or 6 bits, rounded to the nearest step
static inline uint8_t to_bits (uint32_t value, uint32_t max)
{
    return (uint8_t)(((value & 0xFFU) * max + 127U) / 255U);
}

static uint8_t send_chunk (uint32_t length, uint8_t first)
{
    return first ? st7789_memory_write(&s_st7789, s_chunk, (uint16_t)length)
                 : st7789_memory_continue_write(&s_st7789, s_chunk, (uint16_t)length);
}

uint8_t displays_draw_region (uint8_t display, const ui_region_t *region)
{
    if (display >= DISPLAY_COUNT || region == NULL) return 1;
    if (region->x0 < 0 || region->y0 < 0 || region->x0 > region->x1 || region->y0 > region->y1) return 1;
    if (region->x1 >= DISPLAY_SIZE_PX || region->y1 >= DISPLAY_SIZE_PX) return 1;

    const int16_t width = (int16_t)(region->x1 - region->x0 + 1);

    lock();
    select_display(display);
    uint8_t res = st7789_set_column_address(&s_st7789, (uint16_t)region->x0, (uint16_t)region->x1);
    res |= st7789_set_row_address(&s_st7789, (uint16_t)((uint16_t)region->y0 + ROW_OFFSET), (uint16_t)((uint16_t)region->y1 + ROW_OFFSET));

    uint32_t used = 0;
    uint8_t first = 1;
#if DISPLAY_COLOR_BITS == 12
    uint8_t half = 0;     // a pixel's red and green are out, its blue waits for the next pixel
    uint8_t held_blue = 0;
#endif
    for (int16_t y = region->y0; y <= region->y1 && res == 0; y++)
    {
        ui_render_row(region, y, s_row);
        for (int16_t i = 0; i < width; i++)
        {
            const uint32_t c = s_row[i];
#if DISPLAY_COLOR_BITS == 16
            const uint16_t pixel = (uint16_t)((to_bits(c >> 16, 31) << 11) | (to_bits(c >> 8, 63) << 5) | to_bits(c, 31));
            s_chunk[used++] = (uint8_t)(pixel >> 8);
            s_chunk[used++] = (uint8_t)pixel;
#else
            // two pixels share three bytes: R1G1, B1R2, G2B2
            const uint8_t r = to_bits(c >> 16, 15), g = to_bits(c >> 8, 15), b = to_bits(c, 15);
            if (!half)
            {
                s_chunk[used++] = (uint8_t)((r << 4) | g);
                held_blue = b;
                half = 1;
                continue;
            }
            s_chunk[used++] = (uint8_t)((held_blue << 4) | r);
            s_chunk[used++] = (uint8_t)((g << 4) | b);
            half = 0;
#endif
            if (used >= REGION_CHUNK)
            {
                res |= send_chunk(used, first);
                first = 0;
                used = 0;
                if (res) break;
            }
        }
    }
#if DISPLAY_COLOR_BITS == 12
    if (half) s_chunk[used++] = (uint8_t)(held_blue << 4); // an odd pixel count: the last pixel's blue
#endif
    if (used > 0 && res == 0) res |= send_chunk(used, first);
    release_display(display);
    unlock();

    return res ? 1 : 0;
}
