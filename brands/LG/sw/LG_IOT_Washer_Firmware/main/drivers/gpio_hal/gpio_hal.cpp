/*
 * gpio_hal.cpp
 * Hardware abstraction layer for GPIO, PWM (LEDC), and DAC
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gpio_hal.h"
#include "app_config.h"
#include "machine_state.h"
#if CONFIG_SIMULATOR_MODE
#include "simulator.h"
#endif

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/dac_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "../../ulp/ulp_manager.h"

static const char *TAG = "gpio_hal";
static dac_oneshot_handle_t s_dac_handle = nullptr;

/*
 * Why this HAL exists:
 * - Provide a small, testable abstraction over raw GPIO/LEDC/DAC so higher
 *   level code (wash logic, UI) doesn't need to include driver headers or
 *   reason about pin assignments. This makes unit testing and simulator
 *   integration easier because the HAL can route calls to a simulator back-end.
 * - Keep hardware-specific configuration details (pull-ups/pull-downs,
 *   channel assignments) in one place so they are easy to audit and update.
 */

/*===========================================================================
 * GPIO Initialization
 *===========================================================================*/

esp_err_t app_gpio_init(void)
{
    esp_err_t ret;
    
    /*
     * Buttons are configured with an internal pull-down rather than pull-up
     * because the hardware wiring uses a pull-up resistor on the board and
     * we prefer an explicit software pull-down to avoid floating states when
     * the button is open. This clarifies the intended idle logic level for
     * future maintainers and matches the board schematics.
     */
    // Configure input pins (Buttons with Pull-down)
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << PIN_POWER_BUTTON) | 
                        (1ULL << PIN_START_STOP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&button_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure input pins (Sensor without Pull-down, GPIO 34-39 are input only, no internal pull)
    gpio_config_t sensor_conf = {
        .pin_bit_mask = (1ULL << PIN_DOOR_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&sensor_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure sensor GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure digital output pins (start/stop and power LEDs use GPIO directly)
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << PIN_START_STOP_LED) | 
                        (1ULL << PIN_POWER_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&output_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure output GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set initial output states
    gpio_set_level(PIN_POWER_LED, 0);
    gpio_set_level(PIN_START_STOP_LED, 0);
    ESP_LOGI(TAG, "GPIO initialized");
    return ESP_OK;
}

/*===========================================================================
 * LEDC (PWM) Initialization
 *===========================================================================*/

esp_err_t app_ledc_init(void)
{
    esp_err_t ret;
    
    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LEDC_MODE;
    timer_conf.duty_resolution = LEDC_DUTY_RES;
    timer_conf.timer_num = LEDC_TIMER;
    timer_conf.freq_hz = LEDC_FREQUENCY;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Channel configurations
    struct {
        int gpio;
        ledc_channel_t channel;
    } channels[] = {
        { PIN_CIRCULATION_PUMP, LEDC_CH_CIRCULATION },
        { PIN_DRAIN_PUMP,       LEDC_CH_DRAIN },
        { PIN_DRUM_LED,         LEDC_CH_DRUM_LED },
        { PIN_FILL_PUMP,        LEDC_CH_FILL },
    };
    
    for (int i = 0; i < sizeof(channels)/sizeof(channels[0]); i++) {
        ledc_channel_config_t ch_conf = {};
        ch_conf.gpio_num = channels[i].gpio;
        ch_conf.speed_mode = LEDC_MODE;
        ch_conf.channel = channels[i].channel;
        ch_conf.intr_type = LEDC_INTR_DISABLE;
        ch_conf.timer_sel = LEDC_TIMER;
        ch_conf.duty = 0;
        ch_conf.hpoint = 0;
        ret = ledc_channel_config(&ch_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure LEDC channel %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "LEDC (PWM) initialized at %d Hz, %d-bit resolution", 
             LEDC_FREQUENCY, LEDC_DUTY_RES);
    return ESP_OK;
}

/*===========================================================================
 * DAC Initialization
 *===========================================================================*/

esp_err_t app_dac_init(void)
{
    // ESP32 DAC channels: DAC_CHAN_0 = GPIO25, DAC_CHAN_1 = GPIO26
    // We use GPIO25 for speaker
    dac_oneshot_config_t cfg = {};
    cfg.chan_id = DAC_CHAN_0;
    

    esp_err_t ret = dac_oneshot_new_channel(&cfg, &s_dac_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DAC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "DAC initialized on GPIO25 using oneshot driver");
    return ESP_OK;
}

/*===========================================================================
 * GPIO Operations
 *===========================================================================*/

int gpio_read(int pin)
{
#if CONFIG_SIMULATOR_MODE
    return simulator_get_gpio_state(pin);
#else
    return gpio_get_level(static_cast<gpio_num_t>(pin));
#endif
}

void gpio_write(int pin, int level)
{
#if CONFIG_SIMULATOR_MODE
    simulator_send_gpio_state(pin, level);
#endif
    gpio_set_level(static_cast<gpio_num_t>(pin), level);
}

/*===========================================================================
 * PWM (LEDC) Operations
 *===========================================================================*/

static inline void hal_ledc_update(ledc_channel_t channel, uint32_t duty)
{
#if CONFIG_SIMULATOR_MODE
    // Map channel to GPIO for simulation visualization
    int pin = -1;
    switch(channel) {
        case LEDC_CH_CIRCULATION: pin = PIN_CIRCULATION_PUMP; break;
        case LEDC_CH_DRAIN:       pin = PIN_DRAIN_PUMP; break;
        case LEDC_CH_DRUM_LED:    pin = PIN_DRUM_LED; break;
        case LEDC_CH_FILL:        pin = PIN_FILL_PUMP; break;
        default: break;
    }
    if (pin != -1) {
        simulator_send_gpio_state(pin, duty > 0 ? 1 : 0);
    }
#endif
    /*
     * Update the LEDC duty. We call `ledc_update_duty` after setting the
     * duty because on some targets the actual timer update is applied only
     * when `ledc_update_duty` is invoked. Keeping these two calls together
     * avoids subtle timing differences across SDK versions.
     */
    ledc_set_duty(LEDC_MODE, channel, duty);
    ledc_update_duty(LEDC_MODE, channel);
}

void pwm_set_circulation_pump(uint32_t duty)
{
    hal_ledc_update(LEDC_CH_CIRCULATION, duty);
}

void pwm_set_drain_pump(uint32_t duty)
{
    hal_ledc_update(LEDC_CH_DRAIN, duty);
}

void pwm_set_fill_pump(uint32_t duty)
{
    hal_ledc_update(LEDC_CH_FILL, duty);
}

void pwm_set_drum_led(uint32_t duty)
{
    hal_ledc_update(LEDC_CH_DRUM_LED, duty);
}

/*===========================================================================
 * DAC Operations
 *===========================================================================*/

void dac_output(uint8_t value)
{
    if (!s_dac_handle) {
        ESP_LOGW(TAG, "DAC handle not initialized");
        return;
    }
    dac_oneshot_output_voltage(s_dac_handle, value);
}

dac_oneshot_handle_t gpio_hal_get_dac_handle(void)
{
    return s_dac_handle;
}

/*===========================================================================
 * Button Handling
 *===========================================================================*/

// Forward declarations for button handlers (implemented in main)
extern void handle_power_button(void);
extern void handle_start_stop_button(void);
extern void handle_start_stop_long_press(void);

void check_buttons(void)
{
    /* Non-blocking button handling using debounced levels from the ULP
     * coprocessor. The ULP filters noise; here we only look for level
     * transitions to generate short/long press events.
     */
    TickType_t now = xTaskGetTickCount();

    // Door sensor: sample once, keep it simple and fast
    int door_level = gpio_read(PIN_DOOR_SENSOR);
    machine_set_door_open(door_level != 0);

    static int last_power_state = 0;
    static int last_start_state = 0;
    static TickType_t start_press_tick = 0;

    uint32_t mask = ulp_get_button_mask();

    // POWER button (short press) – only reacts on rising edge
    int power_state = static_cast<int>(ulp_button_level(0));
    if (power_state != last_power_state) {
        last_power_state = power_state;
        if (power_state) {
            ESP_LOGI(TAG, "Power button pressed (ULP filtered)");
            handle_power_button();
        }
    }

    // START/STOP button (short vs long press) gated by mask bit1
    if (mask & 0x2) {
        int start_state = static_cast<int>(ulp_button_level(1));
        if (start_state != last_start_state) {
            last_start_state = start_state;
            if (start_state) {
                start_press_tick = now;
            } else {
                uint32_t held_ms = (now - start_press_tick) * portTICK_PERIOD_MS;
                if (held_ms >= 2000) {
                    ESP_LOGI(TAG, "Start/Stop long press ( %u ms )", held_ms);
                    handle_start_stop_long_press();
                } else {
                    ESP_LOGI(TAG, "Start/Stop short press ( %u ms )", held_ms);
                    handle_start_stop_button();
                }
                start_press_tick = 0;
            }
        }
    } else {
        // Masked off: reset state to avoid stale long-press timing
        last_start_state = 0;
        start_press_tick = 0;
    }
}
