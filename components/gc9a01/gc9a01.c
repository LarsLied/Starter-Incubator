#include "gc9a01.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "gc9a01";

#define GC9A01_PCLK_HZ          (40 * 1000 * 1000)
#define GC9A01_FILL_LINES       20      // lines per transfer when filling
#define GC9A01_DRAW_TIMEOUT_MS  100

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_trans_done;
static uint16_t *s_fill_buf;            // GC9A01_WIDTH * GC9A01_FILL_LINES pixels, DMA-capable

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_trans_done, &woken);
    return woken == pdTRUE;
}

esp_err_t gc9a01_init(const gc9a01_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(!s_panel, ESP_ERR_INVALID_STATE, TAG, "already initialized");

    s_trans_done = xSemaphoreCreateBinary();
    s_fill_buf = heap_caps_malloc(GC9A01_WIDTH * GC9A01_FILL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(s_trans_done && s_fill_buf, ESP_ERR_NO_MEM, TAG, "alloc failed");

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = config->sclk_gpio,
        .mosi_io_num = config->mosi_gpio,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_WIDTH * GC9A01_FILL_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(config->spi_host, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "SPI bus init failed");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = config->cs_gpio,
        .dc_gpio_num = config->dc_gpio,
        .spi_mode = 0,
        .pclk_hz = GC9A01_PCLK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_trans_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->spi_host, &io_cfg, &io),
                        TAG, "panel IO init failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = config->rst_gpio,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &panel_cfg, &s_panel), TAG, "panel create failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init failed");
    // Same settings as the esp_lcd_gc9a01 example. Change these if colors
    // look inverted or the image is mirrored on your module.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on failed");

    ESP_LOGI(TAG, "initialized (SCLK=%d MOSI=%d CS=%d DC=%d RST=%d)",
             config->sclk_gpio, config->mosi_gpio, config->cs_gpio, config->dc_gpio, config->rst_gpio);
    return ESP_OK;
}

esp_err_t gc9a01_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(pixels && w > 0 && h > 0, ESP_ERR_INVALID_ARG, TAG, "invalid bitmap");
    ESP_RETURN_ON_FALSE(x + w <= GC9A01_WIDTH && y + h <= GC9A01_HEIGHT, ESP_ERR_INVALID_ARG, TAG, "outside screen");

    xSemaphoreTake(s_trans_done, 0);  // drop any stale event
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, pixels), TAG, "draw failed");

    // Wait for the DMA transfer so the caller may reuse the buffer
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_trans_done, pdMS_TO_TICKS(GC9A01_DRAW_TIMEOUT_MS)) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "draw timeout");
    return ESP_OK;
}

esp_err_t gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(w > 0 && w <= GC9A01_WIDTH, ESP_ERR_INVALID_ARG, TAG, "invalid width");

    const uint16_t be = GC9A01_SWAP16(rgb565);
    const uint16_t lines_per_chunk = GC9A01_FILL_LINES;
    for (int i = 0; i < w * lines_per_chunk; i++) {
        s_fill_buf[i] = be;
    }

    for (uint16_t row = 0; row < h; row += lines_per_chunk) {
        const uint16_t lines = (h - row < lines_per_chunk) ? (h - row) : lines_per_chunk;
        ESP_RETURN_ON_ERROR(gc9a01_draw_bitmap(x, y + row, w, lines, s_fill_buf), TAG, "fill failed");
    }
    return ESP_OK;
}

esp_err_t gc9a01_fill(uint16_t rgb565)
{
    return gc9a01_fill_rect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, rgb565);
}
