/**
 * @file peltier.h
 * @brief Peltier element actuator (heat and cool). STUB in phase 1.
 *
 * The power stage (H-bridge) is not designed yet. This interface hides it:
 * power is a signed percentage, sign selects direction.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t pwm_gpio;    /**< PWM to the H-bridge (LEDC) */
    gpio_num_t dir_gpio;    /**< Direction: heating / cooling */
} peltier_config_t;

/** @brief Configure the power stage. Output is off after init. */
esp_err_t peltier_init(const peltier_config_t *config);

/**
 * @brief Set heating/cooling power.
 * @param percent  -100 = full cooling, 0 = off, +100 = full heating.
 */
esp_err_t peltier_set_power(int8_t percent);

/** @brief Turn the element off (safe state). */
esp_err_t peltier_stop(void);

#ifdef __cplusplus
}
#endif
