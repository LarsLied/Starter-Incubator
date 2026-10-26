/**
 * @file cloud_client.h
 * @brief Publish snapshots to a cloud backend (MQTT or HTTPS, TBD). STUB in phase 1.
 */
#pragma once

#include "esp_err.h"
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Set up the connection to the backend. Requires Wi-Fi. */
esp_err_t cloud_client_init(void);

/** @brief Send one snapshot. Returns an error if offline; caller may retry. */
esp_err_t cloud_client_publish_snapshot(const sensor_snapshot_t *snap);

#ifdef __cplusplus
}
#endif
