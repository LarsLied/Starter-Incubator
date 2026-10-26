// TODO(phase2): rise tracking, peak detection, time-to-ready estimate
#include "fermentation.h"

#include "esp_log.h"

static const char *TAG = "fermentation";

esp_err_t fermentation_init(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t fermentation_update(const sensor_snapshot_t *snap)
{
    (void)snap;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t fermentation_mark_fed(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t fermentation_get_status(fermentation_status_t *out)
{
    (void)out;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
