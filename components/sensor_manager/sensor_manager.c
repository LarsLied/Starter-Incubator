#include "sensor_manager.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "dht22.h"
#include "hx711.h"
#include "jsn_sr04t.h"

static const char *TAG = "sensor_mgr";

#define SENSOR_TASK_STACK       4096
#define SENSOR_TASK_PRIORITY    5
#define DHT22_WARMUP_MS         DHT22_MIN_INTERVAL_MS

static QueueHandle_t s_queue;       // length 1, always holds the newest snapshot
static SemaphoreHandle_t s_hx711_lock;
static TaskHandle_t s_task;

// Set when the driver initialized OK
static bool s_weight_available;
static bool s_climate_available;
static bool s_distance_available;

/*
 * The only place the distance sensor is used. To swap the JSN-SR04T for a
 * ToF sensor (VL53L0X/VL53L1X), change this function and init_distance().
 */
#if CONFIG_SENSOR_DISTANCE_ENABLE
static esp_err_t init_distance(void)
{
    const jsn_sr04t_config_t cfg = {
        .trig_gpio = CONFIG_JSN_SR04T_TRIG_GPIO,
        .echo_gpio = CONFIG_JSN_SR04T_ECHO_GPIO,
    };
    return jsn_sr04t_init(&cfg);
}
#endif

static esp_err_t read_distance(float air_temp_c, uint32_t *distance_mm)
{
#if CONFIG_DISTANCE_TEMP_COMPENSATION
    jsn_sr04t_set_air_temp_c(air_temp_c);
#else
    (void)air_temp_c;
#endif
    return jsn_sr04t_measure_median_mm(CONFIG_DISTANCE_SAMPLES, distance_mm);
}

static void sample_all(sensor_snapshot_t *snap)
{
    esp_err_t err;
    snap->timestamp_us = esp_timer_get_time();

    // Climate first: the temperature is used to compensate the distance reading
    // Errors are passed on in climate_err and reported by the app
    snap->climate_err = s_climate_available
                        ? dht22_read(&snap->temp_c, &snap->humidity_pct)
                        : ESP_ERR_INVALID_STATE;
    snap->climate_valid = (snap->climate_err == ESP_OK);

    if (s_distance_available) {
        const float air_temp = snap->climate_valid ? snap->temp_c : (float)CONFIG_DISTANCE_DEFAULT_TEMP_C;
        err = read_distance(air_temp, &snap->distance_mm);
        snap->distance_valid = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "distance read failed: %s", esp_err_to_name(err));
        }
    }

    if (s_weight_available) {
        xSemaphoreTake(s_hx711_lock, portMAX_DELAY);
        err = hx711_read_average(CONFIG_HX711_SAMPLES, &snap->weight_raw);
        if (err == ESP_OK) {
            snap->weight_g = hx711_raw_to_grams(snap->weight_raw);
        }
        xSemaphoreGive(s_hx711_lock);
        snap->weight_valid = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "HX711 read failed: %s", esp_err_to_name(err));
        }
    }
}

static void sensor_task(void *arg)
{
    // DHT22 needs ~2 s after power-up before the first read
    vTaskDelay(pdMS_TO_TICKS(DHT22_WARMUP_MS));

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        sensor_snapshot_t snap = {0};
        sample_all(&snap);
        xQueueOverwrite(s_queue, &snap);
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_SAMPLE_INTERVAL_MS));
    }
}

esp_err_t sensor_manager_init(void)
{
    s_queue = xQueueCreate(1, sizeof(sensor_snapshot_t));
    ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_NO_MEM, TAG, "queue alloc failed");
    s_hx711_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_hx711_lock, ESP_ERR_NO_MEM, TAG, "mutex alloc failed");

    esp_err_t err;

#if CONFIG_SENSOR_WEIGHT_ENABLE
    const hx711_config_t hx_cfg = {
        .dout_gpio = CONFIG_HX711_DOUT_GPIO,
        .sck_gpio = CONFIG_HX711_SCK_GPIO,
    };
    err = hx711_init(&hx_cfg);
    if (err == ESP_OK) {
        hx711_set_scale(CONFIG_HX711_COUNTS_PER_GRAM_X100 / 100.0f);
#if CONFIG_HX711_TARE_ON_BOOT
        if (hx711_tare(CONFIG_HX711_SAMPLES) != ESP_OK) {
            ESP_LOGW(TAG, "tare at boot failed, weight will be uncalibrated");
        }
#endif
        s_weight_available = true;
    } else {
        ESP_LOGW(TAG, "HX711 init failed: %s", esp_err_to_name(err));
    }
#endif

    const dht22_config_t dht_cfg = {
        .data_gpio = CONFIG_DHT22_GPIO,
#if CONFIG_DHT22_INTERNAL_PULLUP
        .enable_internal_pullup = true,
#endif
    };
    err = dht22_init(&dht_cfg);
    s_climate_available = (err == ESP_OK);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHT22 init failed: %s", esp_err_to_name(err));
    }

#if CONFIG_SENSOR_DISTANCE_ENABLE
    err = init_distance();
    s_distance_available = (err == ESP_OK);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "distance sensor init failed: %s", esp_err_to_name(err));
    }
#endif

    ESP_LOGI(TAG, "initialized (weight=%d climate=%d distance=%d)",
             s_weight_available, s_climate_available, s_distance_available);
    return ESP_OK;
}

esp_err_t sensor_manager_start(void)
{
    ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(!s_task, ESP_ERR_INVALID_STATE, TAG, "already started");

    BaseType_t ok = xTaskCreate(sensor_task, "sensors", SENSOR_TASK_STACK, NULL,
                                SENSOR_TASK_PRIORITY, &s_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");
    return ESP_OK;
}

esp_err_t sensor_manager_wait_next(sensor_snapshot_t *out, TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    return xQueueReceive(s_queue, out, timeout) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t sensor_manager_get_latest(sensor_snapshot_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    return xQueuePeek(s_queue, out, 0) == pdTRUE ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_manager_tare(void)
{
    ESP_RETURN_ON_FALSE(s_weight_available, ESP_ERR_INVALID_STATE, TAG, "scale not available");
    xSemaphoreTake(s_hx711_lock, portMAX_DELAY);
    esp_err_t err = hx711_tare(CONFIG_HX711_SAMPLES);
    xSemaphoreGive(s_hx711_lock);
    return err;
}
