/* ULP manager for power + start/stop button wakeup */
#include <inttypes.h>
#include <stdint.h>
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
static gpio_num_t s_start_gpio = GPIO_NUM_NC;

uint32_t *io_numbers = &ulp_io_numbers;
uint32_t *next_edge = &ulp_next_edge;
uint32_t *debounce_counter = &ulp_debounce_counter;
uint32_t *debounce_max_count = &ulp_debounce_max_count;
uint32_t *edge_count_buttons = &ulp_edge_count_buttons;

static uint32_t s_wake_edges = 1;
static uint32_t s_debounce_samples = 3;
static uint32_t s_wakeup_period_us = 20000; // 20 ms
static uint32_t s_button_mask = 1; // bit0=power, bit1=start
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
                         gpio_num_t start_gpio,
                         uint32_t wake_edges,
                         uint32_t debounce_samples,
                         uint32_t wake_period_us)
{
    s_power_gpio = power_gpio;
    s_start_gpio = start_gpio;
    s_wake_edges = wake_edges ? wake_edges : 1;
    s_debounce_samples = debounce_samples ? debounce_samples : 3;
    s_wakeup_period_us = wake_period_us ? wake_period_us : 20000;

    ESP_RETURN_ON_ERROR(configure_rtc_input(s_power_gpio), TAG, "rtc cfg failed");
    ESP_RETURN_ON_ERROR(configure_rtc_input(s_start_gpio), TAG, "rtc cfg failed");

    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_RETURN_ON_ERROR(err, TAG, "load binary failed");

    int rtcio_num_power = rtc_io_number_get(s_power_gpio);
    int rtcio_num_start = rtc_io_number_get(s_start_gpio);
    io_numbers[0] = rtcio_num_power;
    io_numbers[1] = rtcio_num_start;

    ulp_edge_count_to_wake_up = s_wake_edges;
    debounce_max_count[0] = s_debounce_samples;
    debounce_max_count[1] = s_debounce_samples;
    next_edge[0] = 0;
    next_edge[1] = 0;
    ulp_button_enable_mask = s_button_mask;
    ulp_buttons_clear_counters();

#if CONFIG_IDF_TARGET_ESP32
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
#endif
    esp_deep_sleep_disable_rom_logging();
    s_ulp_loaded = true;
    return ESP_OK;
}

esp_err_t ulp_set_button_mask(uint32_t mask_bits)
{
    // Always ensure power button (bit0) stays enabled
    mask_bits |= 0x1;
    s_button_mask = mask_bits;
    ulp_button_enable_mask = s_button_mask;
    return ESP_OK;
}

uint32_t ulp_get_button_mask(void)
{
    return s_button_mask;
}

esp_err_t ulp_power_arm(void)
{
    if (!s_ulp_loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    ulp_set_button_mask(s_button_mask);
    ulp_buttons_clear_counters();
    next_edge[0] = 0;
    next_edge[1] = 0;
    ESP_RETURN_ON_ERROR(ulp_set_wakeup_period(0, s_wakeup_period_us), TAG, "set period failed");
    esp_err_t err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_RETURN_ON_ERROR(err, TAG, "ulp_run failed");
    return ESP_OK;
}

uint32_t ulp_button_edge_count(int index)
{
    if (index < 0 || index > 1) {
        return 0;
    }
    return (uint32_t)(edge_count_buttons[index] & UINT16_MAX);
}

uint32_t ulp_button_level(int index)
{
    if (index < 0 || index > 1) {
        return 0;
    }
    return (uint32_t)(next_edge[index] & 1);
}

void ulp_buttons_clear_counters(void)
{   
    edge_count_buttons[0] = 0;
    edge_count_buttons[1] = 0;
    debounce_counter[0] = s_debounce_samples;
    debounce_counter[1] = s_debounce_samples;
}

esp_err_t ulp_power_enter_deep_sleep(void)
{
    // Ensure only the power button is armed for wake from deep sleep
    ulp_set_button_mask(0x1);
    ESP_LOGI(TAG, "Arming ULP for power button wake");
    ESP_RETURN_ON_ERROR(ulp_power_arm(), TAG, "arm failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_ulp_wakeup(), TAG, "enable wakeup failed");
    ESP_LOGI(TAG, "Entering deep sleep; waiting for power button");
    esp_deep_sleep_start();
    return ESP_OK; // unreachable in practice
}