/**
  **********************************************************************************
  * UI RENDERER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file ui/ui_render.c
  * @brief See ui_render.h. Text uses the same font tables as the ST7789 driver, read
  *        the same way: each character is size/2 columns, each column top to bottom,
  *        most significant bit first.
  **********************************************************************************
*/

#include "ui/ui_render.h"
#include "hardware_drivers/driver_st7789_font.h"
#include "ui/ui_font_big.h" // the value pop-up's large font, from ui_bigfont_gen.py
#include <stddef.h>
#include <string.h>

/** The character's bitmap and how many bytes each of its columns takes, or NULL for an unknown size. */
static const uint8_t *glyph (uint8_t size, char c, uint8_t *bytes_per_column)
{
    uint8_t index = (uint8_t)c;
    index = (index < ' ' || index > '~') ? 0U : (uint8_t)(index - ' '); // anything else draws as a space

    switch (size)
    {
        case 12: *bytes_per_column = 2; return gsc_st7789_ascii_1206[index];
        case 16: *bytes_per_column = 2; return gsc_st7789_ascii_1608[index];
        case 24: *bytes_per_column = 3; return gsc_st7789_ascii_2412[index];
        case UI_FONT_BIG_SIZE: *bytes_per_column = (UI_FONT_BIG_SIZE + 7) / 8; return UI_FONT_BIG[index];
        default: return NULL;
    }
}

uint8_t ui_big_font_size (void)
{
    return UI_FONT_BIG_SIZE;
}

uint16_t ui_text_width (const char *text, uint8_t size)
{
    return (uint16_t)(strlen(text) * (size / 2U));
}

void ui_render_row (const ui_region_t *region, int16_t y, uint32_t *row)
{
    const int16_t left = region->x0;
    const int16_t width = (int16_t)(region->x1 - region->x0 + 1);

    for (int16_t i = 0; i < width; i++)
    {
        row[i] = region->background;
    }

    if (region->paint != NULL)
    {
        region->paint(region->paint_context, y, row, region->x0, region->x1);
    }

    for (uint8_t r = 0; r < region->rect_count; r++)
    {
        const ui_rect_t *rect = &region->rects[r];
        if (y < rect->y0 || y > rect->y1) continue;
        const int16_t from = rect->x0 > region->x0 ? rect->x0 : region->x0;
        const int16_t to = rect->x1 < region->x1 ? rect->x1 : region->x1;
        for (int16_t x = from; x <= to; x++)
        {
            row[x - left] = rect->rgb;
        }
    }

    for (uint8_t t = 0; t < region->text_count; t++)
    {
        ui_draw_text_row(&region->texts[t], y, row, region->x0, region->x1);
    }
}

void ui_draw_text_row (const ui_text_t *text, int16_t y, uint32_t *row, int16_t x0, int16_t x1)
{
    if (text->text == NULL || y < text->y || y >= text->y + text->size) return;

    const uint8_t glyph_row = (uint8_t)(y - text->y);
    const uint8_t columns = text->size / 2U;
    int16_t char_x = text->x;
    for (const char *c = text->text; *c != '\0'; c++, char_x = (int16_t)(char_x + columns))
    {
        uint8_t bytes_per_column;
        const uint8_t *bitmap = glyph(text->size, *c, &bytes_per_column);
        if (bitmap == NULL) break;
        for (uint8_t col = 0; col < columns; col++)
        {
            const int16_t x = (int16_t)(char_x + col);
            if (x < x0 || x > x1) continue;
            const uint8_t bits = bitmap[col * bytes_per_column + (glyph_row >> 3)];
            if (bits & (0x80U >> (glyph_row & 7U)))
            {
                row[x - x0] = text->rgb;
            }
        }
    }
}
