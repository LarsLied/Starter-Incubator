#include "dht22.h"

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "dht22";

#define DHT22_START_LOW_US      1100    // host start signal, datasheet: >= 1 ms
#define DHT22_RESPONSE_TIMEOUT  100     // us, sensor response phases are ~80 us
#define DHT22_BIT_LOW_TIMEOUT   80      // us, each bit starts with ~50 us low
#define DHT22_BIT_HIGH_TIMEOUT  100     // us, '0' ~26 us high, '1' ~70 us high
#define DHT22_BIT_THRESHOLD_US  40      // high longer than this = '1'
#define DHT22_BITS              40

static dht22_config_t s_cfg;
static bool s_initialized;
static int64_t s_last_attempt_us;
static bool s_cache_valid;
static float s_cached_temp_c;
static float s_cached_rh;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/** Busy-wait until the line reaches @p level. Returns elapsed us, or -1 on timeout. */
static inline int wait_for_level(int level, int timeout_us)
{
    const int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_cfg.data_gpio) != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

/** Run one full transaction and fill the 5 raw bytes. */
static esp_err_t read_frame(uint8_t data[5])
{
    uint8_t high_us[DHT22_BITS];
    bool ok = true;

    // Start signal: pull low for > 1 ms
    gpio_set_level(s_cfg.data_gpio, 0);
    esp_rom_delay_us(DHT22_START_LOW_US);

    // The whole frame takes ~5 ms. Interrupts are disabled on this core so
    // the bit timing is not disturbed.
    // TODO: consider an RMT RX based implementation if this causes problems with Wi-Fi.
    taskENTER_CRITICAL(&s_mux);
    gpio_set_level(s_cfg.data_gpio, 1);  // release, pull-up takes the line high

    // Sensor response: low ~80 us, high ~80 us, then the first bit starts low
    if (wait_for_level(0, DHT22_RESPONSE_TIMEOUT) < 0 ||
        wait_for_level(1, DHT22_RESPONSE_TIMEOUT) < 0 ||
        wait_for_level(0, DHT22_RESPONSE_TIMEOUT) < 0) {
        ok = false;
    }

    for (int i = 0; ok && i < DHT22_BITS; i++) {
        if (wait_for_level(1, DHT22_BIT_LOW_TIMEOUT) < 0) {
            ok = false;
            break;
        }
        int high = wait_for_level(0, DHT22_BIT_HIGH_TIMEOUT);
        if (high < 0) {
            ok = false;
            break;
        }
        high_us[i] = (uint8_t)high;
    }
    taskEXIT_CRITICAL(&s_mux);

    if (!ok) {
        return ESP_ERR_TIMEOUT;
    }

    memset(data, 0, 5);
    for (int i = 0; i < DHT22_BITS; i++) {
        data[i / 8] <<= 1;
        if (high_us[i] > DHT22_BIT_THRESHOLD_US) {
            data[i / 8] |= 1;
        }
    }
    return ESP_OK;
}

esp_err_t dht22_init(const dht22_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    s_cfg = *config;

    const gpio_config_t conf = {
        .pin_bit_mask = 1ULL << s_cfg.data_gpio,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = s_cfg.enable_internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&conf), TAG, "GPIO config failed");
    gpio_set_level(s_cfg.data_gpio, 1);  // idle high

    // Treat init as the last attempt so the sensor gets 2 s to settle after power-up
    s_last_attempt_us = esp_timer_get_time();
    s_cache_valid = false;
    s_initialized = true;

    ESP_LOGI(TAG, "initialized (DATA=%d)", s_cfg.data_gpio);
    return ESP_OK;
}

esp_err_t dht22_read(float *temp_c, float *rh)
{
    ESP_RETURN_ON_FALSE(temp_c && rh, ESP_ERR_INVALID_ARG, TAG, "NULL argument");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    const int64_t now = esp_timer_get_time();
    if (now - s_last_attempt_us < (int64_t)DHT22_MIN_INTERVAL_MS * 1000) {
        if (!s_cache_valid) {
            return ESP_ERR_INVALID_STATE;
        }
        *temp_c = s_cached_temp_c;
        *rh = s_cached_rh;
        return ESP_OK;
    }
    s_last_attempt_us = now;
    s_cache_valid = false;

    uint8_t data[5];
    esp_err_t err = read_frame(data);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) {
        ESP_LOGD(TAG, "checksum mismatch: %02x != %02x", checksum, data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    // Humidity: 16 bit, x10. Temperature: 15 bit magnitude x10, MSB = sign.
    s_cached_rh = (float)((data[0] << 8) | data[1]) / 10.0f;
    float t = (float)(((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    s_cached_temp_c = (data[2] & 0x80) ? -t : t;
    s_cache_valid = true;

    *temp_c = s_cached_temp_c;
    *rh = s_cached_rh;
    return ESP_OK;
}
