/**
 * @file gc9a01.h
 * @brief GC9A01 1.28" round 240x240 TFT driver (4-wire SPI).
 *
 * Thin wrapper around esp_lcd + espressif/esp_lcd_gc9a01. Single instance.
 * Backlight is tied to VCC, so there is no backlight control.
 * Colors are RGB565. Not thread safe: draw from one task only.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC9A01_WIDTH    240
#define GC9A01_HEIGHT   240

/** Swap an RGB565 value to the big-endian byte order the panel expects. */
#define GC9A01_SWAP16(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))

typedef struct {
    spi_host_device_t spi_host;
    gpio_num_t sclk_gpio;   /**< Labelled SCL on most modules */
    gpio_num_t mosi_gpio;   /**< Labelled SDA on most modules */
    gpio_num_t cs_gpio;
    gpio_num_t dc_gpio;
    gpio_num_t rst_gpio;
} gc9a01_config_t;

/** @brief Set up the SPI bus, reset and initialize the panel, turn it on. */
esp_err_t gc9a01_init(const gc9a01_config_t *config);

/** @brief Fill a rectangle with one RGB565 color (native byte order). */
esp_err_t gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565);

/** @brief Fill the whole screen with one RGB565 color (native byte order). */
esp_err_t gc9a01_fill(uint16_t rgb565);

/**
 * @brief Write a w x h bitmap with its top-left corner at (x, y).
 *
 * Blocks until the transfer is done, so the buffer can be reused right away.
 * @param pixels  RGB565 in big-endian order (use GC9A01_SWAP16), in
 *                DMA-capable memory (heap_caps_malloc(..., MALLOC_CAP_DMA)).
 */
esp_err_t gc9a01_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);

#ifdef __cplusplus
}
#endif
