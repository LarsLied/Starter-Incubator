// TODO(phase2): choose transport (MQTT / HTTPS) and implement
#include "cloud_client.h"

#include "esp_log.h"

static const char *TAG = "cloud";

esp_err_t cloud_client_init(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t cloud_client_publish_snapshot(const sensor_snapshot_t *snap)
{
    (void)snap;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
