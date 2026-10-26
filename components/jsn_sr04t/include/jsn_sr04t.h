/**
 * @file jsn_sr04t.h
 * @brief JSN-SR04T waterproof ultrasonic distance sensor driver.
 *
 * 10 us TRIG pulse, ECHO high time = round-trip time of the sound.
 * Echo width is measured with a GPIO interrupt and esp_timer_get_time().
 * Single instance. Units: millimetres, degrees Celsius.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JSN_SR04T_MIN_MM        200     /**< Blind zone, readings below are unreliable */
#define JSN_SR04T_MAX_MM        8000
#define JSN_SR04T_CYCLE_MS      60      /**< Minimum time between two measurements */
#define JSN_SR04T_MAX_SAMPLES   15

typedef struct {
    gpio_num_t trig_gpio;
    gpio_num_t echo_gpio;   /**< Use a voltage divider if the sensor runs at 5 V */
} jsn_sr04t_config_t;

/** @brief Configure TRIG/ECHO and install the echo interrupt. */
esp_err_t jsn_sr04t_init(const jsn_sr04t_config_t *config);

/**
 * @brief Set the air temperature used for speed-of-sound compensation.
 *
 * v = 331.3 + 0.606 * T  [m/s]. Default is 20 C.
 */
void jsn_sr04t_set_air_temp_c(float temp_c);

/**
 * @brief One distance measurement. Calls must be >= JSN_SR04T_CYCLE_MS apart.
 * @param[out] distance_mm  Distance from sensor face to target.
 * @return ESP_OK,
 *         ESP_ERR_TIMEOUT if no echo within ~60 ms,
 *         ESP_ERR_INVALID_RESPONSE if outside JSN_SR04T_MIN_MM..JSN_SR04T_MAX_MM,
 *         ESP_ERR_INVALID_STATE if ECHO is stuck high.
 */
esp_err_t jsn_sr04t_measure_mm(uint32_t *distance_mm);

/**
 * @brief Median of @p samples measurements to reject outliers.
 *
 * Takes about samples * JSN_SR04T_CYCLE_MS. Fails if fewer than half of the
 * samples are valid.
 */
esp_err_t jsn_sr04t_measure_median_mm(uint8_t samples, uint32_t *distance_mm);

#ifdef __cplusplus
}
#endif
