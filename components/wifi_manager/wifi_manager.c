// TODO(phase2): Wi-Fi STA with reconnect, later provisioning
#include "wifi_manager.h"

#include "esp_log.h"

static const char *TAG = "wifi";

esp_err_t wifi_manager_init(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    (void)ssid; (void)password;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

bool wifi_manager_is_connected(void)
{
    return false;
}
