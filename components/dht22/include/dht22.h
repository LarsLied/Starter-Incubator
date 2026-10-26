/**
 * @file dht22.h
 * @brief DHT22 / AM2302 temperature and humidity sensor driver.
 *
 * Single-wire proprietary protocol, bit-banged with microsecond timing.
 * Single instance. Units: degrees Celsius and %RH.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum time between two physical reads of the sensor. */
#define DHT22_MIN_INTERVAL_MS 2000

typedef struct {
    gpio_num_t data_gpio;       /**< Data line (open drain) */
    bool enable_internal_pullup;/**< Weak internal pull-up; external 10 kOhm recommended */
} dht22_config_t;

/** @brief Configure the data GPIO. The first read is allowed 2 s after init. */
esp_err_t dht22_init(const dht22_config_t *config);

/**
 * @brief Read temperature and relative humidity.
 *
 * If called less than DHT22_MIN_INTERVAL_MS after the previous attempt, the
 * last successful values are returned without touching the sensor.
 *
 * @param[out] temp_c  Temperature in degrees Celsius.
 * @param[out] rh      Relative humidity in %.
 * @return ESP_OK,
 *         ESP_ERR_TIMEOUT if the sensor did not respond,
 *         ESP_ERR_INVALID_CRC on checksum error,
 *         ESP_ERR_INVALID_STATE if called too early and no valid cached value exists.
 */
esp_err_t dht22_read(float *temp_c, float *rh);

#ifdef __cplusplus
}
#endif
