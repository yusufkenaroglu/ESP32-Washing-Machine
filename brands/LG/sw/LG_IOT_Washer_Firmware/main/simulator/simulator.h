#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the simulator (WiFi + WebServer)
esp_err_t simulator_init(void);

// Send a draw rectangle command to the web client
void simulator_send_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

// Send a bitmap draw command to the web client
void simulator_send_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data);

// Send a GPIO update to the web client
void simulator_send_gpio_state(int pin, int level);

// Send current motor telemetry (target/current RPM + direction)
void simulator_send_motor_state(int target_rpm, float current_rpm, bool direction_ccw);

// Get the simulated state of a GPIO pin (for inputs)
int simulator_get_gpio_state(int pin);

// Set the simulated state of a GPIO pin (called by the web client)
void simulator_set_gpio_input(int pin, int level);

#ifdef __cplusplus
}
#endif
