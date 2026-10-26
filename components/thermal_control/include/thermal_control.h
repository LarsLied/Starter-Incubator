/**
 * @file thermal_control.h
 * @brief Chamber temperature controller (heat/cool via Peltier). STUB in phase 1.
 *
 * Intended: PID towards a setpoint, both directions, with safety limits and
 * power off when the temperature reading is invalid.
 */
#pragma once

#include "esp_err.h"
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialize the controller and the Peltier. Output starts off. */
esp_err_t thermal_control_init(void);

/** @brief Set target chamber temperature in degrees Celsius. */
esp_err_t thermal_control_set_setpoint_c(float setpoint_c);

/**
 * @brief Run one control step with a new snapshot.
 *
 * Call once per snapshot. Must turn the Peltier off if climate_valid is false.
 */
esp_err_t thermal_control_update(const sensor_snapshot_t *snap);

/** @brief Stop controlling and turn the Peltier off. */
esp_err_t thermal_control_stop(void);

#ifdef __cplusplus
}
#endif
