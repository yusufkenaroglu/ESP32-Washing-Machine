/* ULP manager for power-button wakeup */
#include <inttypes.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "driver/rtc_io.h"
#include "ulp.h"
#include "ulp_main.h"
#include "ulp_manager.h"
#include "app_config.h"

static const char *TAG = "ulp_mgr";

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

static gpio_num_t s_power_gpio = GPIO_NUM_NC;
static uint32_t s_wake_edges = 1;
static uint32_t s_debounce_samples = 3;
static uint32_t s_wakeup_period_us = 20000; // 20 ms
static bool s_ulp_loaded = false;

static esp_err_t configure_rtc_input(gpio_num_t gpio)
{
    if (!rtc_gpio_is_valid_gpio(gpio)) {
        ESP_LOGE(TAG, "GPIO %d is not RTC-capable", (int)gpio);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(rtc_gpio_init(gpio), TAG, "rtc init failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_set_direction(gpio, RTC_GPIO_MODE_INPUT_ONLY), TAG, "dir failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_pullup_dis(gpio), TAG, "pullup dis failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_pulldown_dis(gpio), TAG, "pulldown dis failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_hold_en(gpio), TAG, "hold failed");
    return ESP_OK;
}

esp_err_t ulp_power_init(gpio_num_t power_gpio,
                         uint32_t wake_edges,
                         uint32_t debounce_samples,
                         uint32_t wake_period_us)
{
#if CONFIG_SIMULATOR_MODE
    (void)power_gpio; (void)wake_edges; (void)debounce_samples; (void)wake_period_us;
    return ESP_OK;
#else
    s_power_gpio = power_gpio;
    s_wake_edges = wake_edges ? wake_edges : 1;
    s_debounce_samples = debounce_samples ? debounce_samples : 3;
    s_wakeup_period_us = wake_period_us ? wake_period_us : 20000;

    ESP_RETURN_ON_ERROR(configure_rtc_input(s_power_gpio), TAG, "rtc cfg failed");

    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_RETURN_ON_ERROR(err, TAG, "load binary failed");

    int rtcio_num_power = rtc_io_number_get(s_power_gpio);
    ulp_io_number_power = rtcio_num_power;
    ulp_edge_count_to_wake_up = s_wake_edges;
    ulp_debounce_max_count = s_debounce_samples;
    ulp_next_edge = 0;
    ulp_debounce_counter = s_debounce_samples;
    ulp_edge_count_power = 0;

#if CONFIG_IDF_TARGET_ESP32
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
#endif
    esp_deep_sleep_disable_rom_logging();
    s_ulp_loaded = true;
    return ESP_OK;
#endif
}

esp_err_t ulp_power_arm(void)
{
#if CONFIG_SIMULATOR_MODE
    return ESP_OK;
#else
    if (!s_ulp_loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    ulp_edge_count_power = 0;
    ulp_debounce_counter = s_debounce_samples;
    ulp_next_edge = 0;
    ESP_RETURN_ON_ERROR(ulp_set_wakeup_period(0, s_wakeup_period_us), TAG, "set period failed");
    esp_err_t err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_RETURN_ON_ERROR(err, TAG, "ulp_run failed");
    return ESP_OK;
#endif
}

uint32_t ulp_power_edge_count(void)
{
#if CONFIG_SIMULATOR_MODE
    return 0;
#else
    return (uint32_t)(ulp_edge_count_power & UINT16_MAX);
#endif
}

void ulp_power_clear_counters(void)
{
#if CONFIG_SIMULATOR_MODE
    return;
#else
    ulp_edge_count_power = 0;
    ulp_debounce_counter = s_debounce_samples;
#endif
}

esp_err_t ulp_power_enter_deep_sleep(void)
{
#if CONFIG_SIMULATOR_MODE
    ESP_LOGW(TAG, "Simulator mode: skipping deep sleep");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "Arming ULP for power button wake");
    ESP_RETURN_ON_ERROR(ulp_power_arm(), TAG, "arm failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_ulp_wakeup(), TAG, "enable wakeup failed");
    ESP_LOGI(TAG, "Entering deep sleep; waiting for power button");
    esp_deep_sleep_start();
    return ESP_OK; // unreachable in practice
#endif
}