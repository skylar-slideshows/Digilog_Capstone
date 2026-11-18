/**
  **********************************************************************************
  * UI RENDERER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file ui/ui_render.h
  * @brief Draws one screen region a row at a time: a background colour, then filled
  *        rectangles, then text in the ST7789 driver's fonts. Plain C with no hardware
  *        or RTOS calls, so the same code runs on the board and in the PC preview.
  **********************************************************************************
*/

#ifndef UI_RENDER_H
#define UI_RENDER_H

#include <stdint.h>

#define UI_SCREEN_W 240
#define UI_SCREEN_H 240

/** Colours are 0xRRGGBB, 8 bits each; displays.c converts them to the screens' colour depth. */
#define UI_RGB(r, g, b) ((uint32_t)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

/** A filled rectangle. Corners are inclusive, in screen pixels. */
typedef struct
{
    int16_t x0, y0, x1, y1;
    uint32_t rgb;
} ui_rect_t;

/** One line of text. x, y is the top-left corner; size is 12, 16 or 24 (characters are half as wide). */
typedef struct
{
    int16_t x, y;
    uint8_t size;
    uint32_t rgb;
    const char *text;
} ui_text_t;

/**
 * Optional painter for anything that isn't a rectangle or text, such as a curve. It is called for every
 * row y of the region and may set any of row[0 .. x1-x0], where row[i] is screen column x0 + i.
 */
typedef void (*ui_paint_fn) (const void *context, int16_t y, uint32_t *row, int16_t x0, int16_t x1);

/**
 * A rectangle of the screen to redraw. Everything inside it is drawn: the background, then the painter
 * (if any), then the rectangles in order (later ones on top), then the text. Anything outside it is left alone.
 */
typedef struct
{
    int16_t x0, y0, x1, y1;   //!< the area, corners inclusive
    uint32_t background;
    const ui_rect_t *rects;
    uint8_t rect_count;
    const ui_text_t *texts;
    uint8_t text_count;
    ui_paint_fn paint;        //!< optional, NULL for none
    const void *paint_context;
} ui_region_t;

/**
 * Fills row[0 .. x1-x0] with the colours of screen row y of the region (y between y0 and y1).
 */
void ui_render_row (const ui_region_t *region, int16_t y, uint32_t *row);

/** Width in pixels of text at a font size (12, 16 or 24). */
uint16_t ui_text_width (const char *text, uint8_t size);

#endif
