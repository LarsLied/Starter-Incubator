/**
 * @file wifi_manager.h
 * @brief Wi-Fi station connection to home network. STUB in phase 1.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Init netif, event loop and Wi-Fi in STA mode. Requires NVS. */
esp_err_t wifi_manager_init(void);

/** @brief Connect (and keep reconnecting) to the given network. */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/** @brief True when connected and an IP address has been obtained. */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
