// TODO(phase2): PID heat/cool controller
#include "thermal_control.h"

#include "esp_log.h"
#include "peltier.h"

static const char *TAG = "thermal";

esp_err_t thermal_control_init(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t thermal_control_set_setpoint_c(float setpoint_c)
{
    (void)setpoint_c;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t thermal_control_update(const sensor_snapshot_t *snap)
{
    (void)snap;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t thermal_control_stop(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
