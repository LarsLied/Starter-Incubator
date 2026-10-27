#include "ui.h"

#include <inttypes.h>
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
#define COLOR_DISTANCE  DISPLAY_RGB(220, 130, 255)

// Layout (y in pixels). The screen is round, so keep wide text near the middle.
// Four rows, 48 px apart: label scale 2 (14 px high), value scale 3 (21 px high).
#define LABEL_SCALE     2
#define VALUE_SCALE     3
#define TEMP_LABEL_Y    28
#define TEMP_VALUE_Y    46
#define HUM_LABEL_Y     76
#define HUM_VALUE_Y     94
#define WEIGHT_LABEL_Y  124
#define WEIGHT_VALUE_Y  142
#define DIST_LABEL_Y    172
#define DIST_VALUE_Y    190

static bool s_ready;

void ui_init(void)
{
    esp_err_t err = display_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "display init failed: %s", esp_err_to_name(err));
        return;
    }

    display_clear(COLOR_BG);
    display_draw_text_centered(TEMP_LABEL_Y, "TEMPERATUR", LABEL_SCALE, COLOR_LABEL, COLOR_BG);
    display_draw_text_centered(HUM_LABEL_Y, "FUKTIGHET", LABEL_SCALE, COLOR_LABEL, COLOR_BG);
    display_draw_text_centered(WEIGHT_LABEL_Y, "VEKT", LABEL_SCALE, COLOR_LABEL, COLOR_BG);
    display_draw_text_centered(DIST_LABEL_Y, "AVSTAND", LABEL_SCALE, COLOR_LABEL, COLOR_BG);
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

void ui_show_distance(bool valid, uint32_t distance_mm)
{
    static char s_last[16];
    if (!s_ready) {
        return;
    }

    char dist[16] = "-- mm";
    if (valid) {
        snprintf(dist, sizeof(dist), "%" PRIu32 " mm", distance_mm);
    }

    // Called several times per second; only redraw on change to avoid flicker
    if (strcmp(dist, s_last) == 0) {
        return;
    }
    strcpy(s_last, dist);
    display_draw_text_centered(DIST_VALUE_Y, dist, VALUE_SCALE, COLOR_DISTANCE, COLOR_BG);
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
