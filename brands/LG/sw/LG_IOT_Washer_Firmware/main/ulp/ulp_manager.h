/*
 * ULP manager for low-power power-button wakeup.
 */
#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configure ULP program for power + start buttons; does not start deep sleep.
// - power_gpio: RTC-capable GPIO for power button (always enabled)
// - start_gpio: RTC-capable GPIO for start/stop button (enabled when mask bit set)
esp_err_t ulp_power_init(gpio_num_t power_gpio,
					 gpio_num_t start_gpio,
					 uint32_t wake_edges,
					 uint32_t debounce_samples,
					 uint32_t wake_period_us);

// Enable/disable which buttons the ULP should process (bit0 = power, bit1 = start)
esp_err_t ulp_set_button_mask(uint32_t mask_bits);
uint32_t ulp_get_button_mask(void);

// Arm the ULP program (reset counters and start running it).
esp_err_t ulp_power_arm(void);

// Read and clear debounced edge counts captured by the ULP (index 0=power, 1=start).
uint32_t ulp_button_edge_count(int index);
void ulp_buttons_clear_counters(void);
uint32_t ulp_button_level(int index);

// Backwards-compatible wrappers for power button (index 0)
static inline uint32_t ulp_power_edge_count(void) { return ulp_button_edge_count(0); }
static inline void ulp_power_clear_counters(void) { ulp_buttons_clear_counters(); }

// Arm ULP and enter deep sleep waiting for the power button.
esp_err_t ulp_power_enter_deep_sleep(void);

#ifdef __cplusplus
}
#endif
