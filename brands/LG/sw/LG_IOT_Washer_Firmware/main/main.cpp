/*
 * main.cpp
 * ESP-IDF application entry point for Washing Machine Controller
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "esp_sleep.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "ulp.h"
#include "ulp_manager.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "display.h"
#include "gpio_hal.h"
#include "machine_state.h" 
#include "mpu6050.h"
#include "odrive.h"
#include "sound.h"
#include "tasks.h"
#if CONFIG_SIMULATOR_MODE
#include "simulator.h"
#endif
#if CONFIG_WIFI_ENABLED
#include "drivers/wifi/wifi_manager.h"
#include "drivers/freehome/freehome_manager.h"
#endif

static const char *TAG = "main"; 
static bool s_woke_from_ulp = false;
static uint32_t s_ulp_edge_count = 0;

void handle_power_button(void)
{
    ESP_LOGI(TAG, "Power button pressed");
    tasks_post_simple_event(WM_EVENT_POWER_BUTTON, 0);
}

void handle_start_stop_button(void)
{
    ESP_LOGI(TAG, "Start/Stop button pressed");
    tasks_post_simple_event(WM_EVENT_START_BUTTON, 0);
}

void handle_start_stop_long_press(void)
{
    ESP_LOGI(TAG, "Start/Stop button long pressed");
    tasks_post_simple_event(WM_EVENT_START_LONG_PRESS, 0);
}

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init(); 
    } 
    ESP_ERROR_CHECK(ret);
}

static void init_peripherals(void)
{
    ESP_ERROR_CHECK(app_gpio_init());
    ESP_ERROR_CHECK(app_ledc_init());
    ESP_ERROR_CHECK(app_dac_init());
    ESP_ERROR_CHECK(sound_init());
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(odrive_init());
#if CONFIG_BALANCE_DETECTION
    ESP_ERROR_CHECK(mpu6050_init());
#else
    ESP_LOGI(TAG, "Balance detection disabled; skipping MPU6050 init");
#endif
}

#if CONFIG_SIMULATOR_MODE
static void simulator_draw_rect_cb(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    simulator_send_draw_rect(x, y, w, h, color);
}

static void simulator_draw_bitmap_cb(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data)
{
    simulator_send_bitmap(x, y, w, h, data);
}

static void init_simulator_hooks(void)
{
    esp_err_t err = simulator_init();
    if (err == ESP_OK) {
        display_set_simulator_hook(simulator_draw_rect_cb);
        display_set_simulator_bitmap_hook(simulator_draw_bitmap_cb);
        ESP_LOGI(TAG, "Simulator hooks enabled");
    } else {
        ESP_LOGW(TAG, "Simulator init failed: %s", esp_err_to_name(err));
    }
}
#else
static inline void init_simulator_hooks(void) {}
#endif

extern "C" void app_main(void)
{   
    ESP_LOGI(TAG, "Booting LG washer controller");
    /*
     * Important: if we load the ULP binary before reading the edge counter
     * the ULP initialization will zero the RTC variables (because
     * `ulp_load_binary` reinitializes the ULP memory). Read the wakeup
     * counters first when waking from ULP so we can mirror the button
     * press into the main event flow.
     */
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    s_woke_from_ulp = (cause == ESP_SLEEP_WAKEUP_ULP);

    if (s_woke_from_ulp) {
        /* Read the counter before loading/initializing the ULP binary. */
        s_ulp_edge_count = ulp_power_edge_count();
        ulp_power_clear_counters();
        ESP_LOGI(TAG, "Wake source: ULP power button (edges=%" PRIu32 ")", s_ulp_edge_count);
    }

    /* Only power button enabled while machine is off */
    ESP_ERROR_CHECK(ulp_set_button_mask(0x1));

    /* Now initialise the ULP manager (this will load the binary and
     * prepare the ULP for normal operation). */
    ESP_ERROR_CHECK(ulp_power_init(PIN_POWER_BUTTON, PIN_START_STOP_BUTTON, 1, 3, 20000));

    if (!s_woke_from_ulp) {
        ESP_LOGI(TAG, "Machine is off; arming ULP and entering deep sleep");
        ESP_ERROR_CHECK(ulp_power_enter_deep_sleep());
    }

    init_nvs();
    ESP_ERROR_CHECK(esp_event_loop_create_default());  
    ESP_ERROR_CHECK(machine_state_init());
    // Initialize FreeHome manager and only initialize WiFi if FreeHome is enabled
#if CONFIG_WIFI_ENABLED
    // Initialize FreeHome first (reads persisted state from NVS)
    freehome_init();

    // Only bring up WiFi if FreeHome is enabled (avoids unnecessary WiFi on devices
    // that never opted into cloud features)
    if (freehome_is_enabled()) {
        ESP_ERROR_CHECK(wifi_manager_init());

        // DEV: in non-simulator runs we don't auto-start AP here; UI triggers it
        ESP_LOGI(TAG, "FreeHome enabled; WiFi initialized at boot");
    } else {
        ESP_LOGI(TAG, "FreeHome disabled; skipping WiFi init at boot");
    }
#endif 
    init_peripherals();
    init_simulator_hooks();

    ESP_ERROR_CHECK(tasks_create_all());
    // Keep the ULP program running so we can re-enter deep sleep when powering off

    ESP_ERROR_CHECK(ulp_power_arm());
    if (s_woke_from_ulp && s_ulp_edge_count > 0) {
        // Mirror a power-button press into the existing event flow
        tasks_post_simple_event(WM_EVENT_POWER_BUTTON, 0);
    }
    ESP_LOGI(TAG, "Main task idling; control plane active");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}
