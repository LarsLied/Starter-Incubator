/**
 * @file ui.h
 * @brief What is shown on the round display.
 */
#pragma once

#include "sensor_manager.h"

/** @brief Init the display and draw the static parts. A missing display only logs a warning. */
void ui_init(void);

/** @brief Update temperature and humidity on screen from a snapshot. */
void ui_show_snapshot(const sensor_snapshot_t *snap);

/** @brief Update the weight on screen. Cheap to call often: redraws only on change. */
void ui_show_weight(bool valid, float grams);
