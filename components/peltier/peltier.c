// TODO(phase2): implement once the H-bridge power stage is chosen (LEDC PWM + DIR)
#include "peltier.h"

#include "esp_log.h"

static const char *TAG = "peltier";

esp_err_t peltier_init(const peltier_config_t *config)
{
    (void)config;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t peltier_set_power(int8_t percent)
{
    (void)percent;
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t peltier_stop(void)
{
    ESP_LOGW(TAG, "not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
