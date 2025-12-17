/*
 * gpio_hal.h
 * Hardware abstraction layer for GPIO, PWM (LEDC), and DAC
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/dac_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize all GPIO pins (inputs and outputs)
 * @return ESP_OK on success
 */
esp_err_t app_gpio_init(void);

/**
 * @brief Initialize LEDC (PWM) channels for pumps and LEDs
 * @return ESP_OK on success
 */
esp_err_t app_ledc_init(void);

/**
 * @brief Initialize DAC for audio output
 * @return ESP_OK on success
 */
esp_err_t app_dac_init(void);

/*===========================================================================
 * GPIO Operations
 *===========================================================================*/

/**
 * @brief Read digital input pin
 * @param pin GPIO pin number
 * @return Pin level (0 or 1)
 */
int gpio_read(int pin);

/**
 * @brief Write to digital output pin
 * @param pin GPIO pin number
 * @param level Level to set (0 or 1)
 */
void gpio_write(int pin, int level);

/*===========================================================================
 * PWM (LEDC) Operations
 *===========================================================================*/

/**
 * @brief Set PWM duty cycle for circulation pump
 * @param duty Duty cycle (0-4095 for 12-bit resolution)
 */
void pwm_set_circulation_pump(uint32_t duty);

/**
 * @brief Set PWM duty cycle for drain pump
 * @param duty Duty cycle (0-4095)
 */
void pwm_set_drain_pump(uint32_t duty);

/**
 * @brief Set PWM duty cycle for fill pump
 * @param duty Duty cycle (0-4095)
 */
void pwm_set_fill_pump(uint32_t duty);

/**
 * @brief Set PWM duty cycle for power LED
 * @param duty Duty cycle (0-4095)
 */
void pwm_set_power_led(uint32_t duty);

/**
 * @brief Set PWM duty cycle for drum LED
 * @param duty Duty cycle (0-4095)
 */
void pwm_set_drum_led(uint32_t duty);

/*===========================================================================
 * DAC Operations
 *===========================================================================*/

/**
 * @brief Output voltage on DAC (for audio)
 * @param value 8-bit value (0-255)
 */
void dac_output(uint8_t value);

/**
 * @brief Retrieve underlying DAC oneshot handle
 * @return Handle created during app_dac_init (NULL if not initialized)
 */
dac_oneshot_handle_t gpio_hal_get_dac_handle(void);

/*===========================================================================
 * Button Handling
 *===========================================================================*/

/**
 * @brief Check and debounce button inputs
 * Updates door_is_open, handles power and start/stop button presses
 */
void check_buttons(void);

#ifdef __cplusplus
}
#endif
