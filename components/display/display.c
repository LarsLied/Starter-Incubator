#include "display.h"

#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "font5x7.h"
#include "gc9a01.h"

static const char *TAG = "display";

#define CELL_W  (FONT5X7_WIDTH + 1)     // 1 column spacing between characters

static uint16_t *s_glyph_buf;           // one scaled character cell, DMA-capable

esp_err_t display_init(void)
{
    const gc9a01_config_t cfg = {
        .spi_host = SPI2_HOST,
        .sclk_gpio = CONFIG_GC9A01_SCLK_GPIO,
        .mosi_gpio = CONFIG_GC9A01_MOSI_GPIO,
        .cs_gpio = CONFIG_GC9A01_CS_GPIO,
        .dc_gpio = CONFIG_GC9A01_DC_GPIO,
        .rst_gpio = CONFIG_GC9A01_RST_GPIO,
    };

    const size_t cell_px = (CELL_W * DISPLAY_MAX_SCALE) * (FONT5X7_HEIGHT * DISPLAY_MAX_SCALE);
    s_glyph_buf = heap_caps_malloc(cell_px * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(s_glyph_buf, ESP_ERR_NO_MEM, TAG, "glyph buffer alloc failed");

    return gc9a01_init(&cfg);
}

esp_err_t display_clear(uint16_t color)
{
    return gc9a01_fill(color);
}

esp_err_t display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    // Clip to the screen
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH) { w = DISPLAY_WIDTH - x; }
    if (y + h > DISPLAY_HEIGHT) { h = DISPLAY_HEIGHT - y; }
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }
    return gc9a01_fill_rect(x, y, w, h, color);
}

int display_text_width(const char *text, uint8_t scale)
{
    int chars = 0;
    while (*text) {
        font5x7_next_glyph(&text);
        chars++;
    }
    // No spacing after the last character
    return chars > 0 ? (chars * CELL_W - 1) * scale : 0;
}

static esp_err_t draw_glyph(int x, int y, const uint8_t *glyph, uint8_t scale, uint16_t fg_be, uint16_t bg_be)
{
    const int cell_w = CELL_W * scale;
    const int cell_h = FONT5X7_HEIGHT * scale;

    for (int row = 0; row < FONT5X7_HEIGHT; row++) {
        for (int col = 0; col < CELL_W; col++) {
            const bool on = col < FONT5X7_WIDTH && (glyph[row] & (0x10 >> col));
            const uint16_t color = on ? fg_be : bg_be;
            for (int sy = 0; sy < scale; sy++) {
                uint16_t *dst = &s_glyph_buf[(row * scale + sy) * cell_w + col * scale];
                for (int sx = 0; sx < scale; sx++) {
                    dst[sx] = color;
                }
            }
        }
    }
    return gc9a01_draw_bitmap(x, y, cell_w, cell_h, s_glyph_buf);
}

esp_err_t display_draw_text(int x, int y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg)
{
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    ESP_RETURN_ON_FALSE(s_glyph_buf, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(scale >= 1 && scale <= DISPLAY_MAX_SCALE, ESP_ERR_INVALID_ARG, TAG, "invalid scale");
    ESP_RETURN_ON_FALSE(x >= 0 && y >= 0 && y + DISPLAY_TEXT_HEIGHT(scale) <= DISPLAY_HEIGHT,
                        ESP_ERR_INVALID_ARG, TAG, "text outside screen");

    const uint16_t fg_be = GC9A01_SWAP16(fg);
    const uint16_t bg_be = GC9A01_SWAP16(bg);
    const int cell_w = CELL_W * scale;

    while (*text && x + cell_w <= DISPLAY_WIDTH) {
        const uint8_t *glyph = font5x7_next_glyph(&text);
        ESP_RETURN_ON_ERROR(draw_glyph(x, y, glyph, scale, fg_be, bg_be), TAG, "draw failed");
        x += cell_w;
    }
    return ESP_OK;
}

esp_err_t display_draw_text_centered(int y, const char *text, uint8_t scale, uint16_t fg, uint16_t bg)
{
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    ESP_RETURN_ON_ERROR(display_fill_rect(0, y, DISPLAY_WIDTH, DISPLAY_TEXT_HEIGHT(scale), bg), TAG, "clear failed");

    int x = (DISPLAY_WIDTH - display_text_width(text, scale)) / 2;
    return display_draw_text(x < 0 ? 0 : x, y, text, scale, fg, bg);
}
