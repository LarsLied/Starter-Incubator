#include "jsn_sr04t.h"

#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "jsn_sr04t";

#define JSN_SR04T_TRIGGER_US        10
#define JSN_SR04T_ECHO_TIMEOUT_MS   60

static jsn_sr04t_config_t s_cfg;
static bool s_initialized;
static float s_air_temp_c = 20.0f;
static SemaphoreHandle_t s_echo_done;
static volatile int64_t s_rise_us;
static volatile int64_t s_fall_us;

static void IRAM_ATTR echo_isr(void *arg)
{
    const int64_t now = esp_timer_get_time();
    if (gpio_get_level(s_cfg.echo_gpio)) {
        s_rise_us = now;
    } else if (s_rise_us != 0) {
        s_fall_us = now;
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(s_echo_done, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static int cmp_u32(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a;
    const uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

esp_err_t jsn_sr04t_init(const jsn_sr04t_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    s_cfg = *config;

    s_echo_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_echo_done, ESP_ERR_NO_MEM, TAG, "semaphore alloc failed");

    const gpio_config_t trig_conf = {
        .pin_bit_mask = 1ULL << s_cfg.trig_gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&trig_conf), TAG, "TRIG config failed");
    gpio_set_level(s_cfg.trig_gpio, 0);

    const gpio_config_t echo_conf = {
        .pin_bit_mask = 1ULL << s_cfg.echo_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // keep ECHO low if the sensor is unplugged
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&echo_conf), TAG, "ECHO config failed");

    // The ISR service may already be installed by another component
    esp_err_t err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "ISR service install failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(s_cfg.echo_gpio, echo_isr, NULL), TAG, "ISR handler add failed");

    s_initialized = true;
    ESP_LOGI(TAG, "initialized (TRIG=%d, ECHO=%d)", s_cfg.trig_gpio, s_cfg.echo_gpio);
    return ESP_OK;
}

void jsn_sr04t_set_air_temp_c(float temp_c)
{
    s_air_temp_c = temp_c;
}

esp_err_t jsn_sr04t_measure_mm(uint32_t *distance_mm)
{
    ESP_RETURN_ON_FALSE(distance_mm, ESP_ERR_INVALID_ARG, TAG, "distance_mm is NULL");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    if (gpio_get_level(s_cfg.echo_gpio)) {
        return ESP_ERR_INVALID_STATE;  // previous echo still running or line stuck high
    }

    xSemaphoreTake(s_echo_done, 0);  // drop any stale event
    s_rise_us = 0;
    s_fall_us = 0;

    gpio_set_level(s_cfg.trig_gpio, 1);
    esp_rom_delay_us(JSN_SR04T_TRIGGER_US);
    gpio_set_level(s_cfg.trig_gpio, 0);

    if (xSemaphoreTake(s_echo_done, pdMS_TO_TICKS(JSN_SR04T_ECHO_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    const int64_t echo_us = s_fall_us - s_rise_us;
    const float speed_m_s = 331.3f + 0.606f * s_air_temp_c;
    // m/s == mm/ms, so mm = us * (m/s) / 1000, divided by 2 for the round trip
    const float mm = (float)echo_us * speed_m_s / 2000.0f;

    if (mm < JSN_SR04T_MIN_MM || mm > JSN_SR04T_MAX_MM) {
        ESP_LOGD(TAG, "out of range: %.0f mm (echo %lld us)", mm, echo_us);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *distance_mm = (uint32_t)(mm + 0.5f);
    return ESP_OK;
}

esp_err_t jsn_sr04t_measure_median_mm(uint8_t samples, uint32_t *distance_mm)
{
    ESP_RETURN_ON_FALSE(distance_mm, ESP_ERR_INVALID_ARG, TAG, "distance_mm is NULL");
    ESP_RETURN_ON_FALSE(samples > 0 && samples <= JSN_SR04T_MAX_SAMPLES, ESP_ERR_INVALID_ARG,
                        TAG, "samples must be 1..%d", JSN_SR04T_MAX_SAMPLES);

    uint32_t buf[JSN_SR04T_MAX_SAMPLES];
    uint8_t valid = 0;

    for (uint8_t i = 0; i < samples; i++) {
        if (i > 0) {
            vTaskDelay(pdMS_TO_TICKS(JSN_SR04T_CYCLE_MS));
        }
        if (jsn_sr04t_measure_mm(&buf[valid]) == ESP_OK) {
            valid++;
        }
    }

    if (valid == 0 || valid < (samples + 1) / 2) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    qsort(buf, valid, sizeof(buf[0]), cmp_u32);
    *distance_mm = buf[valid / 2];
    return ESP_OK;
}
