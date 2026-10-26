#include "hx711.h"

#include <inttypes.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hx711";

#define HX711_READY_TIMEOUT_US  (200 * 1000)  // 10 SPS -> new sample every 100 ms
#define HX711_DATA_BITS         24
#define HX711_GAIN_PULSES       1             // 1 extra pulse = channel A, gain 128
#define HX711_CLOCK_HALF_US     1             // SCK high must stay < 60 us

static hx711_config_t s_cfg;
static bool s_initialized;
static int32_t s_offset;
static float s_counts_per_gram = 1.0f;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

esp_err_t hx711_init(const hx711_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    s_cfg = *config;

    const gpio_config_t sck_conf = {
        .pin_bit_mask = 1ULL << s_cfg.sck_gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sck_conf), TAG, "SCK config failed");
    // SCK high for > 60 us powers the HX711 down, so keep it low when idle
    gpio_set_level(s_cfg.sck_gpio, 0);

    const gpio_config_t dout_conf = {
        .pin_bit_mask = 1ULL << s_cfg.dout_gpio,
        .mode = GPIO_MODE_INPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&dout_conf), TAG, "DOUT config failed");

    s_initialized = true;

    // The gain for the next conversion is selected by the pulses after a read,
    // so do one throw-away read to make sure channel A / gain 128 is active.
    int32_t dummy;
    if (hx711_read_raw(&dummy) != ESP_OK) {
        ESP_LOGW(TAG, "no response from HX711 (DOUT=%d, SCK=%d)", s_cfg.dout_gpio, s_cfg.sck_gpio);
    }

    ESP_LOGI(TAG, "initialized (DOUT=%d, SCK=%d)", s_cfg.dout_gpio, s_cfg.sck_gpio);
    return ESP_OK;
}

bool hx711_is_ready(void)
{
    return s_initialized && gpio_get_level(s_cfg.dout_gpio) == 0;
}

esp_err_t hx711_read_raw(int32_t *raw)
{
    ESP_RETURN_ON_FALSE(raw, ESP_ERR_INVALID_ARG, TAG, "raw is NULL");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    const int64_t deadline = esp_timer_get_time() + HX711_READY_TIMEOUT_US;
    while (!hx711_is_ready()) {
        if (esp_timer_get_time() > deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    uint32_t value = 0;

    // Interrupts off only while clocking (~50 us), otherwise an interrupt
    // could stretch SCK high past 60 us and power the chip down.
    taskENTER_CRITICAL(&s_mux);
    for (int i = 0; i < HX711_DATA_BITS; i++) {
        gpio_set_level(s_cfg.sck_gpio, 1);
        esp_rom_delay_us(HX711_CLOCK_HALF_US);
        value = (value << 1) | (uint32_t)gpio_get_level(s_cfg.dout_gpio);
        gpio_set_level(s_cfg.sck_gpio, 0);
        esp_rom_delay_us(HX711_CLOCK_HALF_US);
    }
    for (int i = 0; i < HX711_GAIN_PULSES; i++) {
        gpio_set_level(s_cfg.sck_gpio, 1);
        esp_rom_delay_us(HX711_CLOCK_HALF_US);
        gpio_set_level(s_cfg.sck_gpio, 0);
        esp_rom_delay_us(HX711_CLOCK_HALF_US);
    }
    taskEXIT_CRITICAL(&s_mux);

    // 24-bit two's complement -> int32
    if (value & 0x800000) {
        value |= 0xFF000000;
    }
    *raw = (int32_t)value;
    return ESP_OK;
}

esp_err_t hx711_read_average(uint8_t samples, int32_t *avg)
{
    ESP_RETURN_ON_FALSE(avg, ESP_ERR_INVALID_ARG, TAG, "avg is NULL");
    ESP_RETURN_ON_FALSE(samples > 0, ESP_ERR_INVALID_ARG, TAG, "samples must be > 0");

    int64_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw;
        ESP_RETURN_ON_ERROR(hx711_read_raw(&raw), TAG, "read failed");
        sum += raw;
    }
    *avg = (int32_t)(sum / samples);
    return ESP_OK;
}

esp_err_t hx711_tare(uint8_t samples)
{
    int32_t avg;
    ESP_RETURN_ON_ERROR(hx711_read_average(samples, &avg), TAG, "tare failed");
    s_offset = avg;
    ESP_LOGI(TAG, "tare offset = %" PRId32, s_offset);
    return ESP_OK;
}

void hx711_set_offset(int32_t offset)
{
    s_offset = offset;
}

int32_t hx711_get_offset(void)
{
    return s_offset;
}

esp_err_t hx711_set_scale(float counts_per_gram)
{
    ESP_RETURN_ON_FALSE(counts_per_gram != 0.0f, ESP_ERR_INVALID_ARG, TAG, "scale must be non-zero");
    s_counts_per_gram = counts_per_gram;
    return ESP_OK;
}

float hx711_get_scale(void)
{
    return s_counts_per_gram;
}

float hx711_raw_to_grams(int32_t raw)
{
    return (float)(raw - s_offset) / s_counts_per_gram;
}

esp_err_t hx711_get_grams(uint8_t samples, float *grams)
{
    ESP_RETURN_ON_FALSE(grams, ESP_ERR_INVALID_ARG, TAG, "grams is NULL");
    int32_t avg;
    ESP_RETURN_ON_ERROR(hx711_read_average(samples, &avg), TAG, "read failed");
    *grams = hx711_raw_to_grams(avg);
    return ESP_OK;
}
