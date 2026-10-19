#ifndef DISPLAYS_H
#define DISPLAYS_H

#include <stdint.h>

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
 * display: 0-3. size: 12, 16 or 24 (pixels tall; characters are half as wide). One line per call.
 */
uint8_t displays_draw_text (uint8_t display, uint16_t x, uint16_t y, uint8_t size, const char *text);

#endif