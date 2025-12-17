/*
 * ULP manager for low-power power-button wakeup.
 */
#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configure ULP program for the power button; does not start deep sleep.
esp_err_t ulp_power_init(gpio_num_t power_gpio,
						 uint32_t wake_edges,
						 uint32_t debounce_samples,
						 uint32_t wake_period_us);

// Arm the ULP program (reset counters and start running it).
esp_err_t ulp_power_arm(void);

// Read and clear the debounced edge count captured by the ULP.
uint32_t ulp_power_edge_count(void);
void ulp_power_clear_counters(void);

// Arm ULP and enter deep sleep waiting for the power button.
esp_err_t ulp_power_enter_deep_sleep(void);

#ifdef __cplusplus
}
#endif
