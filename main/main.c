/**
 * @file main.c
 * @brief Startup only: init NVS and services, then start the app task.
 */
#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "sensor_manager.h"
#include "ui.h"

static const char *TAG = "app";

#define APP_TASK_STACK      4096
#define APP_TASK_PRIORITY   4

static void log_snapshot(const sensor_snapshot_t *s)
{
    if (s->climate_valid) {
        ESP_LOGI(TAG, "Temperature: %.1f °C  Humidity: %.1f %%", s->temp_c, s->humidity_pct);
    } else {
        ESP_LOGW(TAG, "DHT22 read failed: %s", esp_err_to_name(s->climate_err));
    }

#if CONFIG_SENSOR_WEIGHT_ENABLE || CONFIG_SENSOR_DISTANCE_ENABLE
    char w[24] = "--", d[16] = "--";
    if (s->weight_valid) {
        snprintf(w, sizeof(w), "%.1f g", s->weight_g);
    }
    if (s->distance_valid) {
        snprintf(d, sizeof(d), "%" PRIu32 " mm", s->distance_mm);
    }
    ESP_LOGI(TAG, "Weight: %s (raw=%" PRId32 ")  Distance: %s", w, s->weight_raw, d);
#endif
}

static void app_task(void *arg)
{
    sensor_snapshot_t snap;
    TickType_t last_wake = xTaskGetTickCount();

    // Runs at the weight rate; full snapshots arrive less often
    for (;;) {
        float grams = 0.0f;
        const bool weight_ok = (sensor_manager_get_weight(&grams) == ESP_OK);
        ui_show_weight(weight_ok, grams);

        uint32_t distance_mm = 0;
        const bool distance_ok = (sensor_manager_get_distance(&distance_mm) == ESP_OK);
        ui_show_distance(distance_ok, distance_mm);

        if (sensor_manager_wait_next(&snap, 0) == ESP_OK) {
            log_snapshot(&snap);
            ui_show_snapshot(&snap);

            // --------------------------------------------------------------
            // TODO(phase2): application logic goes here, e.g.
            //   fermentation_update(&snap);
            //   thermal_control_update(&snap);
            //   cloud_client_publish_snapshot(&snap);
            // --------------------------------------------------------------
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_WEIGHT_INTERVAL_MS));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(sensor_manager_init());
    ESP_ERROR_CHECK(sensor_manager_start());
    ui_init();

    // TODO(phase2): init thermal control, fermentation, Wi-Fi, cloud

    xTaskCreate(app_task, "app", APP_TASK_STACK, NULL, APP_TASK_PRIORITY, NULL);
}
