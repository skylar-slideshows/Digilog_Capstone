#ifndef DISPLAYS_H
#define DISPLAYS_H

#include <stdint.h>

/**
 * Colour depth of all four screens: 16 (RGB565, 65,536 colours) or 12 (RGB444, 4,096 colours).
 * 12-bit sends a quarter fewer bytes per pixel, so whole-screen updates run a third faster.
 * Pictures must be made with the same depth: st7789_imagegen.py --bits.
 */
#define DISPLAY_COLOR_BITS 12

/**
 * A picture made by st7789_imagegen.py, ready to draw with displays_draw_image().
 */
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t bits;          //!< colour depth it was made for; must match DISPLAY_COLOR_BITS
    const uint8_t *pixels; //!< the screens' own byte format, row by row
} display_image_t;

/**
 * Backlight, reset and panel setup for all four screens. Returns 0 on success.
 * Call in main.c USER CODE BEGIN 2, after led_init() (both use TIM1).
 */
uint8_t displays_init (void);

/**
 * Draws a test pattern on every screen: its number, then red, green and blue bars, each
 * labelled with its colour. Shows the chip selects, colour order and colour inversion are right.
 */
void displays_test_pattern (void);

/**
 * Draws white text on a black box as wide as the text, so it replaces old text of the same
 * length. To overwrite a longer old string, pad the new one with spaces. Returns 0 on success.
 * display: 0-3. size: 12, 16 or 24 (pixels tall; characters are half as wide). One line per
 * call: at most 39, 29 or 19 characters at sizes 12, 16 and 24.
 */
uint8_t displays_draw_text (uint8_t display, uint16_t x, uint16_t y, uint8_t size, const char *text);

/**
 * Draws a picture with its top-left corner at x, y, one image pixel to one screen pixel.
 * Returns 0 on success, or 1 if it doesn't fit on the screen or was made for the other colour depth.
 */
uint8_t displays_draw_image (uint8_t display, uint16_t x, uint16_t y, const display_image_t *image);

#endif
