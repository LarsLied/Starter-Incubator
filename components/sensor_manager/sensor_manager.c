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
#define WEIGHT_TASK_STACK       3072
#define WEIGHT_TASK_PRIORITY    5
#define DISTANCE_TASK_STACK     3072
#define DISTANCE_TASK_PRIORITY  5
#define HX711_TARE_SAMPLES      10      // Tare is rare, so average more than a normal reading

/*
 * The DHT22 and HX711 drivers disable interrupts on their core while
 * bit-banging (up to ~5 ms for the DHT22). The JSN-SR04T echo ISR runs on the
 * core that installed it (core 0, from app_main), so keep the bit-banging
 * tasks on the last core or they would delay echo timestamps.
 */
#define BITBANG_CORE            (CONFIG_FREERTOS_NUMBER_OF_CORES - 1)

typedef struct {
    float grams;
    int32_t raw;
    esp_err_t err;                  // ESP_OK when grams/raw are valid
} weight_reading_t;

typedef struct {
    uint32_t mm;
    esp_err_t err;                  // ESP_OK when mm is valid
} distance_reading_t;

static QueueHandle_t s_queue;       // length 1, always holds the newest snapshot
static SemaphoreHandle_t s_hx711_lock;
static TaskHandle_t s_task;
static TaskHandle_t s_weight_task;
static TaskHandle_t s_distance_task;

// Latest fast readings, written by weight_task / distance_task, read by everyone else
static weight_reading_t s_weight = { .err = ESP_ERR_NOT_FOUND };
static distance_reading_t s_distance = { .err = ESP_ERR_NOT_FOUND };
static portMUX_TYPE s_latest_mux = portMUX_INITIALIZER_UNLOCKED;

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
    esp_err_t err = jsn_sr04t_init(&cfg);
#if CONFIG_DISTANCE_TEMP_COMPENSATION
    // Until the first DHT22 reading arrives
    jsn_sr04t_set_air_temp_c((float)CONFIG_DISTANCE_DEFAULT_TEMP_C);
#endif
    return err;
}
#endif

static esp_err_t read_distance(uint32_t *distance_mm)
{
    return jsn_sr04t_measure_median_mm(CONFIG_DISTANCE_SAMPLES, distance_mm);
}

static void sample_all(sensor_snapshot_t *snap)
{
    snap->timestamp_us = esp_timer_get_time();

    // Errors are passed on in climate_err and reported by the app
    snap->climate_err = s_climate_available
                        ? dht22_read(&snap->temp_c, &snap->humidity_pct)
                        : ESP_ERR_INVALID_STATE;
    snap->climate_valid = (snap->climate_err == ESP_OK);

#if CONFIG_DISTANCE_TEMP_COMPENSATION
    // Used by distance_task from its next ping; keeps the last value on a failed read
    if (s_distance_available && snap->climate_valid) {
        jsn_sr04t_set_air_temp_c(snap->temp_c);
    }
#endif

    // Weight and distance are sampled faster in their own tasks; take the latest
    taskENTER_CRITICAL(&s_latest_mux);
    const weight_reading_t w = s_weight;
    const distance_reading_t d = s_distance;
    taskEXIT_CRITICAL(&s_latest_mux);

    if (s_weight_available) {
        snap->weight_g = w.grams;
        snap->weight_raw = w.raw;
        snap->weight_valid = (w.err == ESP_OK);
    }
    if (s_distance_available) {
        snap->distance_mm = d.mm;
        snap->distance_valid = (d.err == ESP_OK);
    }
}

static void weight_task(void *arg)
{
    esp_err_t prev_err = ESP_OK;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        weight_reading_t w = {0};
        xSemaphoreTake(s_hx711_lock, portMAX_DELAY);
        w.err = hx711_read_average(CONFIG_HX711_SAMPLES, &w.raw);
        if (w.err == ESP_OK) {
            w.grams = hx711_raw_to_grams(w.raw);
        }
        xSemaphoreGive(s_hx711_lock);

        taskENTER_CRITICAL(&s_latest_mux);
        s_weight = w;
        taskEXIT_CRITICAL(&s_latest_mux);

        // Log only on change, this runs several times per second
        if (w.err != prev_err) {
            if (w.err != ESP_OK) {
                ESP_LOGW(TAG, "HX711 read failed: %s", esp_err_to_name(w.err));
            } else {
                ESP_LOGI(TAG, "HX711 reading again");
            }
            prev_err = w.err;
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_WEIGHT_INTERVAL_MS));
    }
}

static void distance_task(void *arg)
{
    esp_err_t prev_err = ESP_OK;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        distance_reading_t d = {0};
        d.err = read_distance(&d.mm);

        taskENTER_CRITICAL(&s_latest_mux);
        s_distance = d;
        taskEXIT_CRITICAL(&s_latest_mux);

        // Log only on change, this runs several times per second
        if (d.err != prev_err) {
            if (d.err != ESP_OK) {
                ESP_LOGW(TAG, "distance read failed: %s", esp_err_to_name(d.err));
            } else {
                ESP_LOGI(TAG, "distance reading again");
            }
            prev_err = d.err;
        }

        // The sensor needs a pause after the last ping of the median
        vTaskDelay(pdMS_TO_TICKS(JSN_SR04T_CYCLE_MS));
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_DISTANCE_INTERVAL_MS));
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
        if (hx711_tare(HX711_TARE_SAMPLES) != ESP_OK) {
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

    // DHT22 (in sensor_task) and HX711 bit-bang, see BITBANG_CORE
    BaseType_t ok = xTaskCreatePinnedToCore(sensor_task, "sensors", SENSOR_TASK_STACK, NULL,
                                            SENSOR_TASK_PRIORITY, &s_task, BITBANG_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");

    if (s_weight_available) {
        ok = xTaskCreatePinnedToCore(weight_task, "weight", WEIGHT_TASK_STACK, NULL,
                                     WEIGHT_TASK_PRIORITY, &s_weight_task, BITBANG_CORE);
        ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "weight task create failed");
    }

    if (s_distance_available) {
        ok = xTaskCreate(distance_task, "distance", DISTANCE_TASK_STACK, NULL,
                         DISTANCE_TASK_PRIORITY, &s_distance_task);
        ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "distance task create failed");
    }
    return ESP_OK;
}

esp_err_t sensor_manager_get_weight(float *grams)
{
    ESP_RETURN_ON_FALSE(grams, ESP_ERR_INVALID_ARG, TAG, "grams is NULL");
    if (!s_weight_available) {
        return ESP_ERR_INVALID_STATE;
    }
    taskENTER_CRITICAL(&s_latest_mux);
    const weight_reading_t w = s_weight;
    taskEXIT_CRITICAL(&s_latest_mux);
    if (w.err == ESP_OK) {
        *grams = w.grams;
    }
    return w.err;
}

esp_err_t sensor_manager_get_distance(uint32_t *distance_mm)
{
    ESP_RETURN_ON_FALSE(distance_mm, ESP_ERR_INVALID_ARG, TAG, "distance_mm is NULL");
    if (!s_distance_available) {
        return ESP_ERR_INVALID_STATE;
    }
    taskENTER_CRITICAL(&s_latest_mux);
    const distance_reading_t d = s_distance;
    taskEXIT_CRITICAL(&s_latest_mux);
    if (d.err == ESP_OK) {
        *distance_mm = d.mm;
    }
    return d.err;
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
    esp_err_t err = hx711_tare(HX711_TARE_SAMPLES);
    xSemaphoreGive(s_hx711_lock);
    return err;
}
