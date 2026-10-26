// Private to the display component.
#pragma once

#include <stdint.h>

#define FONT5X7_WIDTH   5
#define FONT5X7_HEIGHT  7

/**
 * Return the glyph for the next character in @p *text and advance the pointer.
 * A glyph is 7 rows; bit 4 of each row is the leftmost pixel.
 * Handles the UTF-8 degree sign (0xC2 0xB0). Unknown characters are blank.
 */
const uint8_t *font5x7_next_glyph(const char **text);
