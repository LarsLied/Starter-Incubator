#include "ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "esp_log.h"

static const char *TAG = "ui";

#define COLOR_BG        DISPLAY_BLACK
#define COLOR_LABEL     DISPLAY_RGB(140, 140, 140)
#define COLOR_TEMP      DISPLAY_RGB(255, 160, 40)
#define COLOR_HUMIDITY  DISPLAY_RGB(80, 190, 255)
#define COLOR_WEIGHT    DISPLAY_RGB(120, 220, 120)

// Layout (y in pixels). The screen is round, so keep wide text near the middle.
// Labels are scale 2 (14 px high), values scale 4 (28 px high).
#define VALUE_SCALE     4
#define TEMP_LABEL_Y    32
#define TEMP_VALUE_Y    50
#define HUM_LABEL_Y     92
#define HUM_VALUE_Y     110
#define WEIGHT_LABEL_Y  152
#define WEIGHT_VALUE_Y  170

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
    display_draw_text_centered(WEIGHT_LABEL_Y, "VEKT", 2, COLOR_LABEL, COLOR_BG);
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

    display_draw_text_centered(TEMP_VALUE_Y, temp, VALUE_SCALE, COLOR_TEMP, COLOR_BG);
    display_draw_text_centered(HUM_VALUE_Y, hum, VALUE_SCALE, COLOR_HUMIDITY, COLOR_BG);
}

void ui_show_weight(bool valid, float grams)
{
    static char s_last[16];
    if (!s_ready) {
        return;
    }

    char weight[16] = "-- g";
    if (valid) {
        // Whole grams: decimals are mostly noise and would not fit near the bottom edge
        snprintf(weight, sizeof(weight), "%.0f g", grams);
    }

    // Called several times per second; only redraw on change to avoid flicker
    if (strcmp(weight, s_last) == 0) {
        return;
    }
    strcpy(s_last, weight);
    display_draw_text_centered(WEIGHT_VALUE_Y, weight, VALUE_SCALE, COLOR_WEIGHT, COLOR_BG);
}
