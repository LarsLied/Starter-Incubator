/**
 * @file display.h
 * @brief Simple drawing service for the round 240x240 display.
 *
 * Fills and scalable text with a built-in 5x7 font. Pins come from Kconfig.
 * Coordinates are pixels, (0, 0) is the top-left corner. Colors are RGB565.
 * Not thread safe: draw from one task only.
 *
 * Supported characters: A-Z (lowercase is shown as uppercase), 0-9,
 * space . - % : / and the UTF-8 degree sign. Others are drawn blank.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDTH       240
#define DISPLAY_HEIGHT      240
#define DISPLAY_MAX_SCALE   8

/** Height of one text line in pixels at the given scale. */
#define DISPLAY_TEXT_HEIGHT(scale) (7 * (scale))

/** Build an RGB565 color from 8-bit red, green, blue. */
#define DISPLAY_RGB(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define DISPLAY_BLACK   DISPLAY_RGB(0, 0, 0)
#define DISPLAY_WHITE   DISPLAY_RGB(255, 255, 255)

/** @brief Initialize the display driver with the Kconfig pins. */
esp_err_t display_init(void);

/** @brief Fill the whole screen. */
esp_err_t display_clear(uint16_t color);

/** @brief Fill a rectangle. Parts outside the screen are clipped. */
esp_err_t display_fill_rect(int x, int y, int w, int h, uint16_t color);

/** @brief Width in pixels of @p text drawn at @p scale. */
int display_text_width(const char *text, uint8_t scale);

/**
 * @brief Draw text with its top-left corner at (x, y).
 *
 * Each character cell is filled with @p bg, so drawing over old text of the
 * same length replaces it. Characters past the right edge are skipped.
 * @param scale  1..DISPLAY_MAX_SCALE, glyph is 5*scale x 7*scale pixels.
 */
esp_err_t display_draw_text(int x, int y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg);

/**
 * @brief Clear the full-width band at @p y and draw @p text centered in it.
 *
 * Use this for values that change length, e.g. "9.5" -> "10.5".
 */
esp_err_t display_draw_text_centered(int y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif
