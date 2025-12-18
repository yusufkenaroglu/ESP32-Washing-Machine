#include "simulator.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "tasks.h"

#include "freertos/semphr.h"

static const char *TAG = "simulator";

// Simulated GPIO state (Inputs)
static uint64_t s_gpio_inputs = 0;
static uint64_t s_gpio_latched = 0; // Latch for short pulses
static portMUX_TYPE s_gpio_lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_uart_mutex = nullptr;

#define SIM_UART_NUM UART_NUM_0
#define BUF_SIZE 1024

/*===========================================================================
 * Serial Input Handling
 *===========================================================================*/

static void simulator_input_task(void *arg)
{
    // We need to re-configure the UART driver to ensure we have a RX buffer
    // and that it's set to the correct baud rate.
    // The default console installation might not have a sufficient RX buffer for uart_read_bytes.
    
    // 1. Try to uninstall the default driver (ignore error if not installed)
    uart_driver_delete(SIM_UART_NUM);

    // 2. Re-install with a proper RX buffer
    uart_config_t uart_config = {};
    uart_config.baud_rate = 1000000,
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    // Install driver: RX buffer = BUF_SIZE*2, TX buffer = 0 (blocking), No event queue
    ESP_ERROR_CHECK(uart_driver_install(SIM_UART_NUM, BUF_SIZE * 2, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(SIM_UART_NUM, &uart_config));
    
    // 3. Re-connect the VFS to this UART so printf/ESP_LOG still works
    esp_vfs_dev_uart_use_driver(SIM_UART_NUM);

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    char line_buf[128];
    int line_pos = 0;
    
    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(SIM_UART_NUM, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)data[i];
                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = 0;
                        // Parse line
                        if (strncmp(line_buf, "$I", 2) == 0) {
                            int pin, val;
                            if (sscanf(line_buf + 2, "%d,%d", &pin, &val) == 2) {
                                simulator_set_gpio_input(pin, val);
                                ESP_LOGI(TAG, "Sim Input: GPIO %d = %d", pin, val);
                            } else {
                                ESP_LOGW(TAG, "Failed to parse: %s", line_buf);
                            }
                        } else if (strncmp(line_buf, "$D", 2) == 0) {
                            int delta = 0;
                            if (sscanf(line_buf + 2, "%d", &delta) == 1 && delta != 0) {
                                tasks_post_dial_delta(delta);
                                ESP_LOGI(TAG, "Sim Input: Dial delta %d", delta);
                            } else {
                                ESP_LOGW(TAG, "Failed to parse dial: %s", line_buf);
                            }
                        }
                        line_pos = 0;
                    }
                } else {
                    if (line_pos < sizeof(line_buf) - 1) {
                        line_buf[line_pos++] = c;
                    } else {
                        // Buffer overflow, reset
                        line_pos = 0;
                    }
                }
            }
        }
    }
    free(data);
    vTaskDelete(nullptr);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

esp_err_t simulator_init(void)
{
    s_uart_mutex = xSemaphoreCreateMutex();
    
    // Create input task
    xTaskCreate(simulator_input_task, "sim_input", 4096, nullptr, 10, nullptr);
    
    // We don't use ESP_LOG here because we changed the baud rate and it might conflict
    // But if we want to see logs on the host, we should ensure they are sent correctly.
    // The host script forwards lines that don't start with $ as logs.
    
    return ESP_OK;
}

void simulator_send_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (s_uart_mutex) xSemaphoreTake(s_uart_mutex, portMAX_DELAY);
    // Format: $R<x>,<y>,<w>,<h>,<color>\n
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "$R%d,%d,%d,%d,%d\n", x, y, w, h, color);
    uart_write_bytes(SIM_UART_NUM, buf, len);
    if (s_uart_mutex) xSemaphoreGive(s_uart_mutex);
}

void simulator_send_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data)
{
    if (s_uart_mutex) xSemaphoreTake(s_uart_mutex, portMAX_DELAY);
    
    // Format: $b<x>,<y>,<w>,<h>\n<binary_data>
    // We use 'b' for binary mode to distinguish from 'B' (hex mode)
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "$b%d,%d,%d,%d\n", x, y, w, h);
    uart_write_bytes(SIM_UART_NUM, buf, len);
    
    // Send raw binary data (w * h * 2 bytes)
    uart_write_bytes(SIM_UART_NUM, (const char*)data, w * h * 2);
    
    if (s_uart_mutex) xSemaphoreGive(s_uart_mutex);
}

void simulator_send_gpio_state(int pin, int level)
{
    if (s_uart_mutex) xSemaphoreTake(s_uart_mutex, portMAX_DELAY);
    // Format: $G<pin>,<val>\n
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "$G%d,%d\n", pin, level ? 1 : 0);
    uart_write_bytes(SIM_UART_NUM, buf, len);
    if (s_uart_mutex) xSemaphoreGive(s_uart_mutex);
}

void simulator_send_motor_state(int target_rpm, float current_rpm, bool direction_ccw)
{
    if (s_uart_mutex) xSemaphoreTake(s_uart_mutex, portMAX_DELAY);
    int current_int = (int)current_rpm;
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "$M%d,%d,%d\n", target_rpm, current_int, direction_ccw ? 1 : 0);
    uart_write_bytes(SIM_UART_NUM, buf, len);
    if (s_uart_mutex) xSemaphoreGive(s_uart_mutex);
}

void simulator_set_gpio_input(int pin, int level)
{
    portENTER_CRITICAL(&s_gpio_lock);
    if (level) {
        s_gpio_inputs |= (1ULL << pin);
        s_gpio_latched |= (1ULL << pin);
    } else {
        s_gpio_inputs &= ~(1ULL << pin);
    }
    portEXIT_CRITICAL(&s_gpio_lock);
}

int simulator_get_gpio_state(int pin)
{
    portENTER_CRITICAL(&s_gpio_lock);
    int val = (s_gpio_inputs >> pin) & 1;
    int latched = (s_gpio_latched >> pin) & 1;
    if (latched) {
        s_gpio_latched &= ~(1ULL << pin); // Clear latch on read
    }
    portEXIT_CRITICAL(&s_gpio_lock);
    return val | latched;
}
