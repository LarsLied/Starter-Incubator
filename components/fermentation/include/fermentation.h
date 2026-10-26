/**
 * @file fermentation.h
 * @brief Starter rise tracking, peak detection and time-to-ready. STUB in phase 1.
 *
 * Intended:
 *  - height_mm = empty_distance_mm - distance_mm
 *  - rise_pct relative to the height at feeding time
 *  - feeding detected as a sudden weight increase (or fermentation_mark_fed())
 *  - peak = rise rate goes to ~0 and starts to fall
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FERMENTATION_STATE_IDLE,    /**< No feeding registered */
    FERMENTATION_STATE_RISING,
    FERMENTATION_STATE_PEAK,    /**< Ready for baking */
    FERMENTATION_STATE_FALLING,
} fermentation_state_t;

typedef struct {
    fermentation_state_t state;
    float height_mm;        /**< Current dough height above jar bottom */
    float rise_pct;         /**< Rise relative to height at feeding, % */
    int64_t fed_at_us;      /**< Timestamp of last feeding, 0 if none */
    int32_t eta_to_peak_s;  /**< Estimated seconds to peak, -1 if unknown */
} fermentation_status_t;

esp_err_t fermentation_init(void);

/** @brief Feed one new snapshot into the model. */
esp_err_t fermentation_update(const sensor_snapshot_t *snap);

/** @brief Manually register a feeding now (resets the rise baseline). */
esp_err_t fermentation_mark_fed(void);

esp_err_t fermentation_get_status(fermentation_status_t *out);

#ifdef __cplusplus
}
#endif
