/**
 * @file sensor_manager.h
 * @brief Periodic sampling of all sensors into one sensor_snapshot_t.
 *
 * Owns the drivers (hx711, dht22, jsn_sr04t), applies calibration and unit
 * conversion. The app layer only uses this header, never the drivers.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One set of readings. Check the *_valid flag before using a value. */
typedef struct {
    int64_t timestamp_us;   /**< esp_timer_get_time() when sampling started */

    float weight_g;         /**< Calibrated weight in grams */
    int32_t weight_raw;     /**< Averaged HX711 counts (for calibration) */
    bool weight_valid;

    float temp_c;           /**< Air temperature, degrees Celsius */
    float humidity_pct;     /**< Relative humidity, % */
    bool climate_valid;     /**< Covers both temp_c and humidity_pct */
    esp_err_t climate_err;  /**< Result of the DHT22 read (ESP_OK when valid) */

    uint32_t distance_mm;   /**< Sensor face to dough surface, median of N samples */
    bool distance_valid;
} sensor_snapshot_t;

/**
 * @brief Initialize all sensor drivers from Kconfig.
 *
 * A sensor that fails to initialize is logged and marked unavailable; its
 * values will always be invalid. Only resource errors make this fail.
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief Start the sampling tasks.
 *
 * Climate and distance are sampled every CONFIG_SAMPLE_INTERVAL_MS. The scale
 * has its own faster task (CONFIG_WEIGHT_INTERVAL_MS); snapshots carry its
 * latest reading.
 */
esp_err_t sensor_manager_start(void);

/**
 * @brief Latest weight from the fast weight task, without waiting.
 * @param[out] grams  Calibrated weight in grams (only written on ESP_OK).
 * @return ESP_OK, ESP_ERR_INVALID_STATE if the scale is not available,
 *         ESP_ERR_NOT_FOUND before the first reading, or the last read error.
 */
esp_err_t sensor_manager_get_weight(float *grams);

/**
 * @brief Block until a new snapshot is available and consume it.
 *
 * Intended for a single consumer task. If the consumer is slower than the
 * sample rate, older snapshots are overwritten (you always get the newest).
 * @return ESP_OK, or ESP_ERR_TIMEOUT if nothing arrived within @p timeout.
 */
esp_err_t sensor_manager_wait_next(sensor_snapshot_t *out, TickType_t timeout);

/**
 * @brief Copy the latest snapshot without consuming it.
 * @return ESP_OK, or ESP_ERR_NOT_FOUND if no snapshot has been taken yet.
 */
esp_err_t sensor_manager_get_latest(sensor_snapshot_t *out);

/** @brief Zero the scale with the current load. Safe to call from any task. */
esp_err_t sensor_manager_tare(void);

#ifdef __cplusplus
}
#endif
