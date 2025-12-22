#include "drivers/led_ring.h"
#include "app_config.h"
#include "machine_state/machine_state.h"
#include "machine_state/constants.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "led_ring";
static led_strip_handle_t s_strip = nullptr;

void program_dial_leds_init(void)
{
    if (s_strip) return;

    led_strip_config_t led_cfg = {
        .strip_gpio_num = PIN_PROGRAM_DIAL,
        .max_leds = NUM_PROGRAMS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = 0 },
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .flags = { .with_dma = 0 },
    };

    esp_err_t err = led_strip_new_rmt_device(&led_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        s_strip = nullptr;
        return;
    }

    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
}

void program_dial_leds_clear(void)
{
    if (!s_strip) return;
    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
}

void program_dial_leds_set_selected(int idx)
{
    if (!s_strip) return;

    if (!machine_is_powered()) {
        program_dial_leds_clear();
        return;
    }

    if (idx < 0 || idx >= (int)NUM_PROGRAMS) {
        program_dial_leds_clear();
        return;
    }

    led_strip_clear(s_strip);

    const uint32_t red = 50;
    const uint32_t green = 50;
    const uint32_t blue = 50;

    esp_err_t err = led_strip_set_pixel(s_strip, (uint32_t)idx, red, green, blue);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_pixel failed idx=%d: %s", idx, esp_err_to_name(err));
    }

    led_strip_refresh(s_strip);
}