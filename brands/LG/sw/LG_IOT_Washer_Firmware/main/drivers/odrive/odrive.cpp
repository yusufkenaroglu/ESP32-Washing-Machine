/*
 * odrive.c
 * ODrive motor controller UART communication
 *
 * Uses ASCII protocol for simplicity and reliability
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Why this module exists:
 * - The odrive module encapsulates UART communication with the motor
 *   controller using a simple ASCII protocol. Keeping the UART framing,
 *   timeouts and parsing inside this module ensures the rest of the system
 *   can interact with the motor using a clean, synchronous API (set
 *   velocity/state) without needing to handle low-level serial details.
 * - The implementation uses a mutex to serialize access which prevents
 *   interleaved commands from different tasks — this trade-off simplifies
 *   the API and is sufficient for our control plane where commands are
 *   relatively infrequent compared to the motor control loop.
 */

#include "odrive.h"
#include "machine_state.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "odrive";

/*===========================================================================
 * State Variables
 *===========================================================================*/

static bool s_initialized = false;
static SemaphoreHandle_t s_uart_mutex = nullptr;

/*===========================================================================
 * Internal Functions
 *===========================================================================*/

/**
 * @brief Send command to ODrive and optionally read response
 * @param cmd Command string (without newline)
 * @param response Buffer for response (can be NULL)
 * @param response_len Length of response buffer
 * @param timeout_ms Timeout in ms
 * @return ESP_OK on success
 */
static esp_err_t odrive_send_command(const char *cmd, char *response, 
                                      size_t response_len, uint32_t timeout_ms)
{
#if CONFIG_SIMULATOR_MODE
    if (response != nullptr && response_len > 0) {
        snprintf(response, response_len, "0.0");
    }
    return ESP_OK;
#endif

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Flush RX buffer
    uart_flush_input(ODRIVE_UART_NUM);
    
    // Send command with newline
    char cmd_buf[ODRIVE_BUF_SIZE];
    int len = snprintf(cmd_buf, sizeof(cmd_buf), "%s\n", cmd);
    
    int written = uart_write_bytes(ODRIVE_UART_NUM, cmd_buf, len);
    if (written < 0) {
        xSemaphoreGive(s_uart_mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "TX: %s", cmd);
    
    // Read response if buffer provided
    if (response != nullptr && response_len > 0) {
        memset(response, 0, response_len);
        
        int total_read = 0;
        int64_t start = esp_timer_get_time();
        
        while (total_read < response_len - 1) {
            int available = 0;
            uart_get_buffered_data_len(ODRIVE_UART_NUM, (size_t *)&available);
            
            if (available > 0) {
                int to_read = (available < (response_len - 1 - total_read)) ? 
                              available : (response_len - 1 - total_read);
                int read = uart_read_bytes(ODRIVE_UART_NUM, 
                                           (uint8_t *)response + total_read,
                                           to_read, pdMS_TO_TICKS(10));
                if (read > 0) {
                    total_read += read;
                    
                    // Check for newline (end of response)
                    if (strchr(response, '\n') != nullptr) {
                        break;
                    }
                }
            }
            
            // Check timeout
            if ((esp_timer_get_time() - start) > (timeout_ms * 1000)) {
                xSemaphoreGive(s_uart_mutex);
                ESP_LOGW(TAG, "Response timeout");
                return ESP_ERR_TIMEOUT;
            }
            
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        // Remove trailing newline
        char *nl = strchr(response, '\n');
        if (nl) *nl = '\0';
        
        ESP_LOGD(TAG, "RX: %s", response);
    }
    
    xSemaphoreGive(s_uart_mutex);
    return ESP_OK;
}

/**
 * @brief Send command and parse float response
 */
static esp_err_t odrive_read_float(const char *cmd, float *value)
{
    char response[64];
    esp_err_t ret = odrive_send_command(cmd, response, sizeof(response), 100);
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    *value = strtof(response, nullptr);
    return ESP_OK;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

esp_err_t odrive_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

#if CONFIG_SIMULATOR_MODE
    ESP_LOGI(TAG, "ODrive disabled in Simulator Mode");
    s_initialized = true;
    return ESP_OK;
#endif
    
    // Create mutex
    s_uart_mutex = xSemaphoreCreateMutex();
    if (s_uart_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Configure UART
    uart_config_t uart_config = {};
    uart_config.baud_rate = ODRIVE_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    esp_err_t ret = uart_param_config(ODRIVE_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_set_pin(ODRIVE_UART_NUM, ODRIVE_TX_PIN, ODRIVE_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_driver_install(ODRIVE_UART_NUM, ODRIVE_BUF_SIZE * 2, 
                              ODRIVE_BUF_SIZE * 2, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "ODrive UART initialized on TX=%d, RX=%d @ %d baud",
             ODRIVE_TX_PIN, ODRIVE_RX_PIN, ODRIVE_BAUD_RATE);
    
    return ESP_OK;
}

esp_err_t odrive_set_state(uint8_t axis, odrive_axis_state_t state)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "w axis%d.requested_state %d", axis, (int)state);
    return odrive_send_command(cmd, nullptr, 0, 100);
}

esp_err_t odrive_set_control_mode(uint8_t axis, odrive_control_mode_t mode)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "w axis%d.controller.config.control_mode %d", 
             axis, (int)mode);
    return odrive_send_command(cmd, nullptr, 0, 100);
}

esp_err_t odrive_set_velocity(uint8_t axis, float velocity)
{
#if CONFIG_SIMULATOR_MODE
    int rpm = (int)(velocity * 60.0f);
    machine_set_target_rpm(abs(rpm));
    machine_set_current_rpm(abs(rpm)); // Instant response for sim
    machine_set_motor_dir(rpm < 0); // True if negative (CCW?)
#endif
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "v %d %.3f 0", axis, velocity);
    return odrive_send_command(cmd, nullptr, 0, 100);
}

esp_err_t odrive_set_velocity_ff(uint8_t axis, float velocity, float torque_ff)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "v %d %.3f %.3f", axis, velocity, torque_ff);
    return odrive_send_command(cmd, nullptr, 0, 100);
}

esp_err_t odrive_get_velocity(uint8_t axis, float *velocity)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "r axis%d.encoder.vel_estimate", axis);
    return odrive_read_float(cmd, velocity);
}

esp_err_t odrive_get_current(uint8_t axis, float *current)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "r axis%d.motor.current_control.Iq_measured", axis);
    return odrive_read_float(cmd, current);
}

esp_err_t odrive_get_bus_voltage(float *voltage)
{
    return odrive_read_float("r vbus_voltage", voltage);
}

esp_err_t odrive_emergency_stop(void)
{
    // Set both axes to idle immediately
    odrive_send_command("w axis0.requested_state 1", nullptr, 0, 50);
    odrive_send_command("w axis1.requested_state 1", nullptr, 0, 50);
    
    ESP_LOGW(TAG, "Emergency stop activated");
    return ESP_OK;
}

esp_err_t odrive_clear_errors(uint8_t axis)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "w axis%d.error 0", axis);
    esp_err_t ret = odrive_send_command(cmd, nullptr, 0, 100);
    
    snprintf(cmd, sizeof(cmd), "w axis%d.motor.error 0", axis);
    odrive_send_command(cmd, nullptr, 0, 100);
    
    snprintf(cmd, sizeof(cmd), "w axis%d.encoder.error 0", axis);
    odrive_send_command(cmd, nullptr, 0, 100);
    
    snprintf(cmd, sizeof(cmd), "w axis%d.controller.error 0", axis);
    odrive_send_command(cmd, nullptr, 0, 100);
    
    return ret;
}

bool odrive_is_connected(void)
{
    if (!s_initialized) {
        return false;
    }
    
    float voltage;
    esp_err_t ret = odrive_get_bus_voltage(&voltage);
    
    if (ret == ESP_OK && voltage > 10.0f) {
        return true;
    }
    
    return false;
}
