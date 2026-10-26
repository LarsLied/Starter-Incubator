#include "ui.h"

#include <stdbool.h>
#include <stdio.h>

#include "display.h"
#include "esp_log.h"

static const char *TAG = "ui";

#define COLOR_BG        DISPLAY_BLACK
#define COLOR_LABEL     DISPLAY_RGB(140, 140, 140)
#define COLOR_TEMP      DISPLAY_RGB(255, 160, 40)
#define COLOR_HUMIDITY  DISPLAY_RGB(80, 190, 255)

// Layout (y in pixels). The screen is round, so keep wide text near the middle.
#define TEMP_LABEL_Y    50
#define TEMP_VALUE_Y    70      // scale 5 -> 35 px high
#define HUM_LABEL_Y     125
#define HUM_VALUE_Y     145     // scale 4 -> 28 px high

static bool s_ready;

void ui_init(void)
{
    esp_err_t err = display_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "display init failed: %s", esp_err_to_name(err));
        return;
    }

    display_clear(COLOR_BG);
    display_draw_text_centered(TEMP_LABEL_Y, "TEMPERATUR", 2, COLOR_LABEL, COLOR_BG);
    display_draw_text_centered(HUM_LABEL_Y, "FUKTIGHET", 2, COLOR_LABEL, COLOR_BG);
    s_ready = true;
}

void ui_show_snapshot(const sensor_snapshot_t *snap)
{
    if (!s_ready) {
        return;
    }

    char temp[16] = "--.-°C";
    char hum[16] = "--.-%";
    if (snap->climate_valid) {
        snprintf(temp, sizeof(temp), "%.1f°C", snap->temp_c);
        snprintf(hum, sizeof(hum), "%.1f%%", snap->humidity_pct);
    }

    display_draw_text_centered(TEMP_VALUE_Y, temp, 5, COLOR_TEMP, COLOR_BG);
    display_draw_text_centered(HUM_VALUE_Y, hum, 4, COLOR_HUMIDITY, COLOR_BG);
}
