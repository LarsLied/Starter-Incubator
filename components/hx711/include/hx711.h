/**
 * @file hx711.h
 * @brief HX711 24-bit load cell ADC driver (channel A, gain 128).
 *
 * Bit-banged 2-wire interface (DOUT, SCK). Single instance.
 * Units: raw values are ADC counts, weights are grams.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t dout_gpio;   /**< Data out from HX711 (input on ESP32) */
    gpio_num_t sck_gpio;    /**< Serial clock / power down (output on ESP32) */
} hx711_config_t;

/**
 * @brief Configure the GPIOs and select channel A / gain 128.
 *
 * Does not fail if the HX711 does not respond yet; that shows up as
 * ESP_ERR_TIMEOUT on the first read.
 */
esp_err_t hx711_init(const hx711_config_t *config);

/** @brief True when a new conversion is available (DOUT low). */
bool hx711_is_ready(void);

/**
 * @brief Read one 24-bit conversion, sign-extended.
 *
 * Waits up to ~200 ms for the HX711 to become ready.
 * @param[out] raw  Signed ADC counts (no offset or scale applied).
 * @return ESP_OK, ESP_ERR_TIMEOUT if the chip never became ready.
 */
esp_err_t hx711_read_raw(int32_t *raw);

/**
 * @brief Average of @p samples raw readings.
 * @param samples  Number of readings, >= 1. At 10 SPS each takes ~100 ms.
 * @param[out] avg Averaged raw counts.
 */
esp_err_t hx711_read_average(uint8_t samples, int32_t *avg);

/** @brief Take the current average as zero (stores it as offset). */
esp_err_t hx711_tare(uint8_t samples);

/** @brief Set / get the tare offset in raw counts (e.g. restored from NVS). */
void hx711_set_offset(int32_t offset);
int32_t hx711_get_offset(void);

/**
 * @brief Set the calibration factor.
 * @param counts_per_gram  Raw counts per gram, must be non-zero.
 */
esp_err_t hx711_set_scale(float counts_per_gram);
float hx711_get_scale(void);

/** @brief Convert raw counts to grams using the current offset and scale. */
float hx711_raw_to_grams(int32_t raw);

/** @brief Read an average and return it in grams. */
esp_err_t hx711_get_grams(uint8_t samples, float *grams);

#ifdef __cplusplus
}
#endif
