/**
  **********************************************************************************
  * DISPLAY - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/lt7683_display.c
  * @brief See lt7683_display.h.
  **********************************************************************************
*/

#include "display/lt7683_display.h"
#include "display/ui_console_test.h"
#include "LT768_Lib.h"
#include "lvgl.h"
#include "CONFIG.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim1; // channel 3 (PC2, Disp_PWM) is the backlight

#define DISP_W LCD_XSIZE_TFT // 1024 (LT768_Lib.h)
#define DISP_H LCD_YSIZE_TFT // 600

// LVGL draws this many lines at a time; each strip then goes to the LT7683
#define DRAW_BUF_LINES 10
static uint16_t draw_buf[DISP_W * DRAW_BUF_LINES] __attribute__((aligned(4))); // 20 KB

static uint32_t     display_stack[1536]; // 6 KB: LVGL's software renderer
static StaticTask_t display_tcb;


/*=============================== LT7683 ================================*/

/** Backlight brightness, 0-100 %. The module's backlight follows BL_CONTROL (its J27 default). */
static void backlight (uint8_t percent)
{
    const uint32_t top = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (top * percent) / 100U);
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
}

static void lt7683_start (void)
{
    Parallel_Init(); // FMC timings for the LT7683
    LT768_Init();    // reset, PLL, SDRAM, panel timing (1024 x 600, RGB565)
    Display_ON();

    // show and draw on the same 1024 x 600 image at the start of the LT7683's SDRAM
    Select_Main_Window_16bpp();
    Main_Image_Start_Address(0);
    Main_Image_Width(DISP_W);
    Main_Window_Start_XY(0, 0);
    Canvas_Image_Start_address(0);
    Canvas_image_width(DISP_W);
    Active_Window_XY(0, 0);
    Active_Window_WH(DISP_W, DISP_H);

    LT768_DrawSquare_Fill(0, 0, DISP_W - 1, DISP_H - 1, Black); // clear, before the light comes on
    backlight(100);
}


/*=============================== LVGL ================================*/

static uint32_t tick_ms (void)
{
    return (uint32_t)xTaskGetTickCount(); // the FreeRTOS tick is 1 ms
}

/** LVGL finished a strip: write it into the LT7683's frame memory */
static void flush_cb (lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const unsigned short w = (unsigned short)(area->x2 - area->x1 + 1);
    const unsigned short h = (unsigned short)(area->y2 - area->y1 + 1);
    MPU16_16bpp_Memory_Write((unsigned short)area->x1, (unsigned short)area->y1, w, h, (const unsigned short *)px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_start (void)
{
    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_display_create(DISP_W, DISP_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    ui_console_test_create();
}


/*=============================== TASK ================================*/

static void display_task (void *argument)
{
    (void)argument;

    lt7683_start();
    lvgl_start();
    printf("\r\nDisplay: LT7683 set up, LVGL running\n");

    for (;;)
    {
        ui_console_test_update();           // moves and bucket status into the widgets
        uint32_t wait = lv_timer_handler(); // draws whatever changed; says when it next needs to run
        if (wait < 5U) wait = 5U;
        if (wait > 50U) wait = 50U;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

void display_start (void)
{
    static const osThreadAttr_t attr = {
        .name = "display",
        .stack_mem = display_stack,
        .stack_size = sizeof(display_stack),
        .cb_mem = &display_tcb,
        .cb_size = sizeof(display_tcb),
        .priority = osPriorityLow, // below the knobs, LEDs, USB: it only gets the time they leave
    };
    if (osThreadNew(display_task, NULL, &attr) == NULL)
    {
        printf("\r\nDisplay task FAILED to start\n");
    }
}
