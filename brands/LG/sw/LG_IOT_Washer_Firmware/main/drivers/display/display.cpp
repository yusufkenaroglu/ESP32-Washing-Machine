/*
 * display.cpp
 * TFT display driver using SPI
 *
 * This is a simplified driver. For production, consider using
 * LVGL or esp_lcd component for better performance.
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#include "display.h"
#include "graphic_assets.h"
#include "fonts.h"
#include "machine_state.h"
#include "constants.h" // For program names if available
#include "ui_controller.h"
#include "drivers/freehome/freehome_manager.h"
#include "drivers/wifi/wifi_manager.h"

#include "esp_wifi.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cstdlib>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "qrcodegen.hpp"

static const char *TAG = "display";

// Sprite dimensions are defined in `app_config.h` (via SPRITE_WIDTH/
// SPRITE_HEIGHT). Avoid duplicating values here.

static uint16_t *s_sprite_buf = nullptr;
static uint16_t *s_prev_sprite_buf = nullptr;

static void (*s_simulator_draw_cb)(int16_t, int16_t, int16_t, int16_t, uint16_t) = nullptr;
static void (*s_simulator_bitmap_cb)(int16_t, int16_t, int16_t, int16_t, const uint16_t *) = nullptr;

void display_set_simulator_hook(void (*cb)(int16_t, int16_t, int16_t, int16_t, uint16_t))
{
    s_simulator_draw_cb = cb;
}

void display_set_simulator_bitmap_hook(void (*cb)(int16_t, int16_t, int16_t, int16_t, const uint16_t *))
{
    s_simulator_bitmap_cb = cb;
}

/*===========================================================================
 * State Variables
 *===========================================================================*/

static spi_device_handle_t s_spi = nullptr;
static bool s_initialized = false;

/*===========================================================================
 * ST7789 Commands
 *===========================================================================*/

#define CMD_NOP 0x00
#define CMD_SWRESET 0x01
#define CMD_SLPIN 0x10
#define CMD_SLPOUT 0x11
#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_DISPOFF 0x28
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_MADCTL 0x36
#define CMD_COLMOD 0x3A

/*===========================================================================
 * SPI Communication
 *===========================================================================*/

static void display_send_cmd(uint8_t cmd)
{
    spi_transaction_t trans = {};
    trans.length = 8;
    trans.tx_buffer = &cmd;
    gpio_set_level(DISPLAY_PIN_DC, 0); // Command mode
    spi_device_transmit(s_spi, &trans);
}

static void display_send_data(const uint8_t *data, size_t len)
{
    if (len == 0)
        return;

    spi_transaction_t trans = {};
    trans.length = len * 8;
    trans.tx_buffer = data;
    gpio_set_level(DISPLAY_PIN_DC, 1); // Data mode
    spi_device_transmit(s_spi, &trans);
}

static void display_send_data8(uint8_t data)
{
    display_send_data(&data, 1);
}

static void display_send_data16(uint16_t data)
{
    uint8_t buf[2] = {
        static_cast<uint8_t>(data >> 8),
        static_cast<uint8_t>(data & 0xFF)};
    display_send_data(buf, 2);
}

static void display_set_window(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    display_send_cmd(CMD_CASET);
    display_send_data16(x0);
    display_send_data16(x1);

    display_send_cmd(CMD_RASET);
    display_send_data16(y0);
    display_send_data16(y1);

    display_send_cmd(CMD_RAMWR);
}

/*===========================================================================
 * Initialization
 *===========================================================================*/

esp_err_t display_init(void)
{
    if (s_initialized)
    {
        return ESP_OK;
    }

    // Configure GPIO
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << DISPLAY_PIN_DC) | (1ULL << DISPLAY_PIN_RST);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Configure SPI
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = DISPLAY_PIN_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = DISPLAY_PIN_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2;

    esp_err_t ret = spi_bus_initialize(DISPLAY_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 20 * 1000 * 1000; // 20 MHz
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = DISPLAY_PIN_CS;
    dev_cfg.queue_size = 7;

    ret = spi_bus_add_device(DISPLAY_SPI_HOST, &dev_cfg, &s_spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Hardware reset
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize display (ST7789)
    display_send_cmd(CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    display_send_cmd(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    display_send_cmd(CMD_COLMOD);
    display_send_data8(0x55); // 16-bit color

    display_send_cmd(CMD_MADCTL);
    display_send_data8(0x00); // Normal orientation

    display_send_cmd(CMD_INVON); // Inversion on (for ST7789)

    display_send_cmd(CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Configure backlight PWM
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_8_BIT;
    timer_conf.timer_num = LEDC_TIMER_1;
    timer_conf.freq_hz = 5000;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {};
    ch_conf.gpio_num = DISPLAY_PIN_BL;
    ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_conf.channel = LEDC_CHANNEL_7;
    ch_conf.timer_sel = LEDC_TIMER_1;
    ch_conf.duty = 255;
    ch_conf.hpoint = 0;
    ledc_channel_config(&ch_conf);

    s_initialized = true;
    ESP_LOGI(TAG, "Display initialized (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Clear screen
    display_clear(COLOR_BLACK);

    // Allocate sprite buffer
    s_sprite_buf = static_cast<uint16_t *>(heap_caps_malloc(SPRITE_WIDTH * SPRITE_HEIGHT * 2, MALLOC_CAP_DMA));
    if (!s_sprite_buf)
    {
        ESP_LOGE(TAG, "Failed to allocate sprite buffer");
        // Fallback to non-DMA memory if DMA failed
        s_sprite_buf = static_cast<uint16_t *>(malloc(SPRITE_WIDTH * SPRITE_HEIGHT * 2));
    }
    if (s_sprite_buf)
    {
        ESP_LOGI(TAG, "Sprite buffer allocated");
    }

    // Allocate previous sprite buffer for differential update
    s_prev_sprite_buf = static_cast<uint16_t *>(malloc(SPRITE_WIDTH * SPRITE_HEIGHT * 2));
    if (s_prev_sprite_buf)
    {
        memset(s_prev_sprite_buf, 0, SPRITE_WIDTH * SPRITE_HEIGHT * 2);
    }

    return ESP_OK;
}

/*===========================================================================
 * Sprite Functions
 *===========================================================================*/

static void sprite_set_pixel(int x, int y, uint16_t color)
{
    if (x >= 0 && x < SPRITE_WIDTH && y >= 0 && y < SPRITE_HEIGHT)
    {
        // Store in Big Endian for direct SPI transfer
        s_sprite_buf[y * SPRITE_WIDTH + x] = (color >> 8) | (color << 8);
    }
}

static void sprite_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    // Clip to sprite boundaries
    if (x >= SPRITE_WIDTH || y >= SPRITE_HEIGHT)
        return;
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > SPRITE_WIDTH)
        w = SPRITE_WIDTH - x;
    if (y + h > SPRITE_HEIGHT)
        h = SPRITE_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;

    uint16_t be_color = (color >> 8) | (color << 8);
    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            s_sprite_buf[(y + row) * SPRITE_WIDTH + (x + col)] = be_color;
        }
    }
}

static void sprite_clear(uint16_t color)
{
    sprite_fill_rect(0, 0, SPRITE_WIDTH, SPRITE_HEIGHT, color);
}

static void sprite_draw_bitmap(int x, int y, int w, int h, const uint16_t *data)
{
    // Clip
    int src_x = 0, src_y = 0;
    (void)src_x;
    (void)src_y;
    if (x < 0)
    {
        src_x = -x;
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        src_y = -y;
        h += y;
        y = 0;
    }
    if (x + w > SPRITE_WIDTH)
        w = SPRITE_WIDTH - x;
    if (y + h > SPRITE_HEIGHT)
        h = SPRITE_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;

    // Data is assumed to be Little Endian (native), we need to store Big Endian

    // Simplified: Assume no negative coordinates for now (icons are placed carefully)
    // and clip right/bottom.
    int draw_w = w;
    int draw_h = h;
    if (x + draw_w > SPRITE_WIDTH)
        draw_w = SPRITE_WIDTH - x;
    if (y + draw_h > SPRITE_HEIGHT)
        draw_h = SPRITE_HEIGHT - y;

    for (int row = 0; row < draw_h; row++)
    {
        for (int col = 0; col < draw_w; col++)
        {
            uint16_t pixel = data[row * w + col];
            s_sprite_buf[(y + row) * SPRITE_WIDTH + (x + col)] = (pixel >> 8) | (pixel << 8);
        }
    }
}

static void sprite_fill_circle(int x, int y, int r, uint16_t color)
{
    if (r <= 0)
        return;
    // For each scanline in the circle, compute horizontal span and fill using sprite_fill_rect
    for (int cy = -r; cy <= r; cy++)
    {
        int dx = static_cast<int>(sqrtf((float)(r * r - cy * cy)));
        int sx = x - dx;
        int width = dx * 2 + 1;
        sprite_fill_rect(sx, y + cy, width, 1, color);
    }
}

// Draw a QR code into the sprite at (x,y) with pixel size 'size'.
// The QR will be drawn with black modules on white background.
static void sprite_draw_qr(int x, int y, int size, const char *text)
{
    if (!text)
        return;
    // Generate QR using minimal encoder
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text, qrcodegen::QrCode::Ecc::LOW);
    int qsize = qr.getSize();
    if (qsize <= 0)
        return;

    // module pixel size (fit into size)
    int module_px = size / qsize;
    if (module_px <= 0)
        return;
    int total_px = module_px * qsize;
    int margin = (size - total_px) / 2;

    // Clear background area
    sprite_fill_rect(x, y, size, size, COLOR_WHITE);

    for (int row = 0; row < qsize; ++row)
    {
        for (int col = 0; col < qsize; ++col)
        {
            if (qr.getModule(col, row))
            {
                int px = x + margin + col * module_px;
                int py = y + margin + row * module_px;
                sprite_fill_rect(px, py, module_px, module_px, COLOR_BLACK);
            }
        }
    }
}

static void sprite_push(int x, int y)
{
    if (!s_sprite_buf)
        return;

    // Check for changes if we have a previous buffer
    bool changed = true;
    if (s_prev_sprite_buf)
    {
        if (memcmp(s_sprite_buf, s_prev_sprite_buf, SPRITE_WIDTH * SPRITE_HEIGHT * 2) == 0)
        {
            changed = false;
        }
        else
        {
            // Update previous buffer
            memcpy(s_prev_sprite_buf, s_sprite_buf, SPRITE_WIDTH * SPRITE_HEIGHT * 2);
        }
    }

    // Send to simulator only if changed
    if (changed && s_simulator_bitmap_cb)
    {
        // Simulator expects Big Endian (Network Byte Order) for direct hex conversion
        // s_sprite_buf is already Big Endian (for SPI)
        s_simulator_bitmap_cb(x, y, SPRITE_WIDTH, SPRITE_HEIGHT, s_sprite_buf);
    }

    // Send to Display
    display_set_window(x, y, x + SPRITE_WIDTH - 1, y + SPRITE_HEIGHT - 1);
    gpio_set_level(DISPLAY_PIN_DC, 1);

    // Send the whole buffer using DMA
    // Max transfer size was set to W*H*2, so we can send in one go
    spi_transaction_t trans = {};
    trans.length = SPRITE_WIDTH * SPRITE_HEIGHT * 16;
    trans.tx_buffer = s_sprite_buf;
    spi_device_transmit(s_spi, &trans);
}

/*===========================================================================
 * Basic Drawing Functions
 *===========================================================================*/

void display_clear(uint16_t color)
{
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (s_simulator_draw_cb)
    {
        s_simulator_draw_cb(x, y, w, h, color);
    }

    if (!s_initialized)
        return;
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
        return;
    if (x + w > DISPLAY_WIDTH)
        w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT)
        h = DISPLAY_HEIGHT - y;

    display_set_window(x, y, x + w - 1, y + h - 1);

    // Prepare color buffer
    uint8_t color_hi = color >> 8;
    uint8_t color_lo = color & 0xFF;

// Send in chunks
#define CHUNK_SIZE 512
    uint8_t chunk[CHUNK_SIZE];
    for (int i = 0; i < CHUNK_SIZE; i += 2)
    {
        chunk[i] = color_hi;
        chunk[i + 1] = color_lo;
    }

    int total_pixels = w * h;
    gpio_set_level(DISPLAY_PIN_DC, 1);

    while (total_pixels > 0)
    {
        int pixels_to_send = (total_pixels > CHUNK_SIZE / 2) ? CHUNK_SIZE / 2 : total_pixels;
        spi_transaction_t trans = {};
        trans.length = pixels_to_send * 16;
        trans.tx_buffer = chunk;
        spi_device_transmit(s_spi, &trans);
        total_pixels -= pixels_to_send;
    }
}

void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    display_fill_rect(x, y, w, 1, color);         // Top
    display_fill_rect(x, y + h - 1, w, 1, color); // Bottom
    display_fill_rect(x, y, 1, h, color);         // Left
    display_fill_rect(x + w - 1, y, 1, h, color); // Right
}

void display_set_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (s_simulator_draw_cb)
    {
        s_simulator_draw_cb(x, y, 1, 1, color);
    }

    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT)
        return;
    display_set_window(x, y, x, y);
    display_send_data16(color);
}

void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        display_set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int16_t e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void display_draw_circle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t cx = 0;
    int16_t cy = r;

    display_set_pixel(x, y + r, color);
    display_set_pixel(x, y - r, color);
    display_set_pixel(x + r, y, color);
    display_set_pixel(x - r, y, color);

    while (cx < cy)
    {
        if (f >= 0)
        {
            cy--;
            ddF_y += 2;
            f += ddF_y;
        }
        cx++;
        ddF_x += 2;
        f += ddF_x;

        display_set_pixel(x + cx, y + cy, color);
        display_set_pixel(x - cx, y + cy, color);
        display_set_pixel(x + cx, y - cy, color);
        display_set_pixel(x - cx, y - cy, color);
        display_set_pixel(x + cy, y + cx, color);
        display_set_pixel(x - cy, y + cx, color);
        display_set_pixel(x + cy, y - cx, color);
        display_set_pixel(x - cy, y - cx, color);
    }
}

void display_fill_circle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    for (int16_t cy = -r; cy <= r; cy++)
    {
        int16_t cx = (int16_t)sqrtf(r * r - cy * cy);
        display_fill_rect(x - cx, y + cy, 2 * cx + 1, 1, color);
    }
}

/*===========================================================================
 * Text Drawing (VLW Font)
 *===========================================================================*/

static uint32_t read_u32(const uint8_t *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void sprite_draw_text_vlw(int x, int y, const char *text, const uint8_t *font, uint16_t color)
{
    if (!font || !text)
        return;

    // Parse Header
    uint32_t count = read_u32(font + 0);
    uint32_t ascent = read_u32(font + 16);

    uint32_t header_size = 24;
    uint32_t glyph_size = 28;
    uint32_t bitmap_start = header_size + count * glyph_size;

    int cx = x;
    int cy = y;

    while (*text)
    {
        uint32_t unicode = (uint8_t)*text++;

        // Find glyph
        uint32_t g_idx = 0;
        bool found = false;
        const uint8_t *g_ptr = font + header_size;

        for (uint32_t i = 0; i < count; i++)
        {
            uint32_t g_unicode = read_u32(g_ptr);
            if (g_unicode == unicode)
            {
                g_idx = i;
                found = true;
                break;
            }
            g_ptr += glyph_size;
        }

        if (!found)
        {
            if (unicode == 32)
            {
                cx += read_u32(font + 8) / 4; // Space width
            }
            continue;
        }

        // Read Metrics
        uint32_t g_height = read_u32(g_ptr + 4);
        uint32_t g_width = read_u32(g_ptr + 8);
        uint32_t g_advance = read_u32(g_ptr + 12);
        int32_t g_dy = (int32_t)read_u32(g_ptr + 16);
        int32_t g_dx = (int32_t)read_u32(g_ptr + 20);

        // Calculate Bitmap Offset
        uint32_t bitmap_offset = 0;
        const uint8_t *p = font + header_size;
        for (uint32_t i = 0; i < g_idx; i++)
        {
            uint32_t h = read_u32(p + 4);
            uint32_t w = read_u32(p + 8);
            bitmap_offset += w * h;
            p += glyph_size;
        }

        const uint8_t *bitmap = font + bitmap_start + bitmap_offset;

        // Draw Bitmap
        int bx = cx + g_dx;
        int by = cy + ascent - g_dy;

        for (int r = 0; r < g_height; r++)
        {
            for (int c = 0; c < g_width; c++)
            {
                uint8_t alpha = bitmap[r * g_width + c];
                if (alpha > 127)
                {
                    sprite_set_pixel(bx + c, by + r, color);
                }
            }
        }

        cx += g_advance;
    }
}

static void sprite_draw_text(int x, int y, const char *text, display_font_t font, uint16_t fg_color, uint16_t bg_color)
{
    const uint8_t *font_data = nullptr;
    switch (font)
    {
        break;
    case FONT_SMALL_BOLD:
        font_data = LGSmartBold15;
        break;
    case FONT_MEDIUM:
        font_data = LGSmart20;
        break;
    case FONT_LARGE:
        font_data = LGSmart24;
        break;
    case FONT_XLARGE:
        font_data = LGSmart28;
        break;
    case FONT_XXLARGE:
        font_data = LGSmart32;
        break;
    }

    if (font_data)
    {
        sprite_draw_text_vlw(x, y, text, font_data, fg_color);
    }
}

void display_draw_text(int16_t x, int16_t y, const char *text,
                       display_font_t font, uint16_t fg_color, uint16_t bg_color)
{
    // Direct drawing not supported for VLW fonts in this simplified driver
    // Use sprite_draw_text instead
}

void display_draw_text_aligned(display_rect_t rect, const char *text,
                               display_font_t font, display_align_t align,
                               uint16_t fg_color, uint16_t bg_color)
{
    // Not implemented for direct drawing
}

/*===========================================================================
 * Bitmap Drawing
 *===========================================================================*/

void display_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data)
{
    if (s_simulator_bitmap_cb)
    {
        s_simulator_bitmap_cb(x, y, w, h, data);
    }

    if (!s_initialized || data == nullptr)
        return;

    display_set_window(x, y, x + w - 1, y + h - 1);

    // Send pixel data (already in RGB565 format)
    gpio_set_level(DISPLAY_PIN_DC, 1);

    // Need to swap bytes for SPI (big endian)
    int total = w * h;
    for (int i = 0; i < total; i++)
    {
        display_send_data16(data[i]);
    }
}

void display_draw_bitmap_mono(int16_t x, int16_t y, int16_t w, int16_t h,
                              const uint8_t *data, uint16_t fg_color, uint16_t bg_color)
{
    if (!s_initialized || data == nullptr)
        return;

    int byte_width = (w + 7) / 8;

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            int byte_idx = row * byte_width + col / 8;
            int bit_idx = 7 - (col % 8);
            uint16_t color = (data[byte_idx] & (1 << bit_idx)) ? fg_color : bg_color;
            display_set_pixel(x + col, y + row, color);
        }
    }
}

/*===========================================================================
 * Backlight Control
 *===========================================================================*/

void display_set_backlight(uint8_t brightness)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_7, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_7);
}

uint16_t display_get_width(void)
{
    return DISPLAY_WIDTH;
}

uint16_t display_get_height(void)
{
    return DISPLAY_HEIGHT;
}

/*===========================================================================
 * Washing Machine UI
 *===========================================================================*/

/*===========================================================================
 * Washing Machine UI
 *===========================================================================*/

static void draw_list_item(int y, bool selected, const char *label, const char *value)
{
    uint16_t fg = COLOR_BLACK;
    uint16_t bg = selected ? COLOR_WHITE : COLOR_BGROUND;
    if (selected)
    {
        sprite_fill_rect(5, y - 2, SPRITE_WIDTH - 10, 20, bg);
    }
    sprite_draw_text(10, y, label, FONT_MEDIUM, fg, bg);
    if (value && value[0] != '\0')
    {
        sprite_draw_text(SPRITE_WIDTH - 80, y, value, FONT_MEDIUM, fg, bg);
    }
}

static int list_start_idx(int cursor, int count)
{
    // Show up to 4 rows; keep cursor visible
    int start = cursor - 3;
    if (start < 0)
        start = 0;
    if (count > 4 && start > count - 4)
        start = count - 4;
    return start < 0 ? 0 : start;
}

static void draw_popup_border(int x, int y, int w, int h)
{
    sprite_fill_rect(x, y, w, h, COLOR_WHITE);
    // Border
    sprite_fill_rect(x, y, w, 1, COLOR_BLACK);
    sprite_fill_rect(x, y + h - 1, w, 1, COLOR_BLACK);
    sprite_fill_rect(x, y, 1, h, COLOR_BLACK);
    sprite_fill_rect(x + w - 1, y, 1, h, COLOR_BLACK);
}

static void draw_wash_option_popup(const ui_render_state_t &ui_state)
{
    // Only show when in wash settings and actively editing the focused item
    if (ui_state.menu != UI_MENU_WASH_SETTINGS || !ui_state.editing)
        return;
    int idx = ui_state.wash_cursor;

    const char *options[6] = {};
    int count = 0;
    int selected_idx = -1;
    char extra_opts[4][4];

    switch (idx)
    {
    case UI_WASH_OPTION_TEMPERATURE:
        options[0] = temperatures[0];
        options[1] = temperatures[1];
        options[2] = temperatures[2];
        options[3] = temperatures[3];
        options[4] = temperatures[4];
        count = 5;
        selected_idx = machine_get_temp_idx();
        break;
    case UI_WASH_OPTION_SPIN:
        options[0] = spin_speeds[0];
        options[1] = spin_speeds[1];
        options[2] = spin_speeds[2];
        options[3] = spin_speeds[3];
        options[4] = spin_speeds[4];
        count = 5;
        selected_idx = machine_get_spin_idx();
        break;
    case UI_WASH_OPTION_SOIL:
        options[0] = soil_levels[0];
        options[1] = soil_levels[1];
        options[2] = soil_levels[2];
        options[3] = soil_levels[3];
        count = 4;
        selected_idx = machine_get_soil_idx();
        break;
    case UI_WASH_OPTION_PREWASH:
        options[0] = "Off";
        options[1] = "On";
        count = 2;
        selected_idx = machine_is_prewash_enabled() ? 1 : 0;
        break;
    case UI_WASH_OPTION_EXTRA_RINSE:
        for (int i = 0; i < 4; ++i)
        {
            snprintf(extra_opts[i], sizeof(extra_opts[i]), "%d", i);
            options[i] = extra_opts[i];
        }
        count = 4;
        selected_idx = machine_get_extra_rinse_count();
        break;
    default:
        return; // navigation rows don't need popup
    }

    if (count <= 0)
        return;
    if (selected_idx < 0 || selected_idx >= count)
        selected_idx = 0;

    const int row_h = 16;
    int popup_h = row_h * count + 6;
    int popup_w = SPRITE_WIDTH - 12;
    int popup_x = 6;
    int popup_y = 12;
    if (popup_y + popup_h > SPRITE_HEIGHT - 2)
    {
        popup_y = SPRITE_HEIGHT - popup_h - 2;
        if (popup_y < 0)
            popup_y = 0;
    }

    draw_popup_border(popup_x, popup_y, popup_w, popup_h);

    int text_y = popup_y + 3;
    for (int i = 0; i < count; ++i)
    {
        int line_y = text_y + i * row_h;
        if (i == selected_idx)
        {
            sprite_fill_rect(popup_x + 2, line_y - 2, popup_w - 4, row_h - 2, COLOR_BGROUND);
        }
        if (options[i])
        {
            sprite_draw_text(popup_x + 6, line_y, options[i], FONT_MEDIUM, COLOR_BLACK, COLOR_WHITE);
        }
    }
}

static void draw_wash_settings(const ui_render_state_t &ui_state)
{
    sprite_clear(COLOR_BGROUND);
    sprite_draw_text(4, 4, "Wash Settings", FONT_LARGE, COLOR_BLACK, COLOR_BGROUND);
    int y = 32;
    int start = list_start_idx(ui_state.wash_cursor, UI_WASH_OPTION_COUNT);
    int drawn = 0;
    for (int i = start; i < UI_WASH_OPTION_COUNT && drawn < 4; ++i, ++drawn)
    {
        char value[24];
        ui_wash_option_value_string(i, value, sizeof(value));
        bool selected = (ui_state.wash_cursor == i);
        draw_list_item(y, selected, ui_wash_option_label(i), value);
        y += 18;
    }
    draw_wash_option_popup(ui_state);
}

static void draw_machine_settings(const ui_render_state_t &ui_state)
{
    sprite_clear(COLOR_BGROUND);
    sprite_draw_text(4, 4, "Settings", FONT_LARGE, COLOR_BLACK, COLOR_BGROUND);
    int y = 32;
    int start = list_start_idx(ui_state.machine_cursor, UI_MACHINE_OPTION_COUNT);
    int drawn = 0;
    for (int i = start; i < UI_MACHINE_OPTION_COUNT && drawn < 4; ++i, ++drawn)
    {
        bool selected = (ui_state.machine_cursor == i);
        draw_list_item(y, selected, ui_machine_option_label(i), "");
        y += 18;
    }
}

static void draw_freehome_menu(const ui_render_state_t &ui_state)
{
    sprite_clear(COLOR_BGROUND);

    // Simple wizard pages
    int page = ui_state.freehome_page;
    int btn = ui_state.freehome_button;

    // Provisioning state for page==2 (show QR). We start the provisioning AP
    // when the user reaches page 2 and keep a local flag to avoid repeating
    // the start request every frame. We also check the AP's station list and
    // auto-advance the wizard to page 3 when a client connects to the AP.
    static bool s_prov_ap_started = false;
    bool client_connected = false;

    // If we're not on the QR page, allow the start flag to reset so the
    // provisioning AP will be requested again when the user returns to page 2.
    if (page != 2) {
        s_prov_ap_started = false;
    }

    if (page == 0)
    {
        // Welcome
        sprite_draw_text(4, 4, "Welcome to FreeHome", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
        sprite_draw_text(24, 30, "Connect and control", FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
        sprite_draw_text(20, 50, " your washer remotely.", FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
    }
    else if (page == 1)
    {
        // Account info & registration QR (placeholder)
        sprite_draw_text(4, 4, "Create an account", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
        // Registration QR (URL with device id)
        //const char *dev = freehome_get_device_id();
        char regurl[37];
        snprintf(regurl, sizeof(regurl), "https://freehome.us.kg/auth/register");
        sprite_draw_qr(50, 20, 64, regurl);
    }
    else if (page == 2)
    {
        // AP QR
        sprite_draw_text(4, 4, "Join Wi-Fi AP", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
        // Build WiFi provisioning QR in standard WiFi QR format
        char wifi_qr[128];
        // Use open AP provisioning
        snprintf(wifi_qr, sizeof(wifi_qr), "WIFI:T:nopass;S:%s;;", WIFI_AP_SSID);
        sprite_draw_qr(10, 10, 64, wifi_qr);

        // Start provisioning AP once when entering this page
        if (!s_prov_ap_started)
        {
            // Request AP regardless of Kconfig guard — the function is available
            // via our wifi_manager integration. Log the request for diagnostics.
            ESP_LOGI(TAG, "Requesting provisioning AP (page 2 entered)");
            esp_err_t _ret = wifi_start_ap_open();
            if (_ret != ESP_OK) {
                ESP_LOGW(TAG, "wifi_start_ap_open() failed: %s", esp_err_to_name(_ret));
            } else {
                ESP_LOGI(TAG, "Provisioning AP requested successfully");
            }
            s_prov_ap_started = true;
        }

        // Check if any station/client has connected to our AP. If so, advance
        // the FreeHome wizard to page 3 so we can wait for credentials to be
        // submitted via the provisioning UI. Use esp_wifi_ap_get_sta_list.
        wifi_sta_list_t sta_list = {};
        if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
            if (sta_list.num > 0) {
                client_connected = true;
            }
        }
    }
    else if (page == 3)
    {
        // Waiting for credentials — show spinner until AP mode active
        sprite_draw_text(4, 4, "Receiving credentials", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
        // Show WiFi status
        wifi_info_t info = {};
        wifi_get_info(&info);

        if (info.status != WIFI_STATUS_AP_MODE)
        {
            // Draw animated spinner to indicate we're setting up AP
            static int spinner_phase = 0;
            spinner_phase = (spinner_phase + 1) % 8;
            const int cx = SPRITE_WIDTH / 2;
            const int cy = 50;
            const int radius = 18;
            const int dot_r = 3;

            // Clear spinner area
            sprite_fill_rect(cx - radius - 4, cy - radius - 4, (radius + 4) * 2, (radius + 4) * 2, COLOR_BGROUND);

            for (int i = 0; i < 8; ++i)
            {
                float ang = (2.0f * 3.14159265f * i) / 8.0f;
                int dx = static_cast<int>(cosf(ang) * radius);
                int dy = static_cast<int>(sinf(ang) * radius);
                int x = cx + dx;
                int y = cy + dy;
                if (i == spinner_phase)
                {
                    sprite_fill_circle(x, y, dot_r, COLOR_BGROUND);
                }
                else
                {
                    sprite_fill_circle(x, y, dot_r, COLOR_BLACK);
                }
            }
        }
        else
        {
            // AP is active — show SSID/IP so user can connect
            char st[64];
            snprintf(st, sizeof(st), "AP active: %s", info.ssid);
            sprite_draw_text(10, 66, st, FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
            char ipbuf[32];
            snprintf(ipbuf, sizeof(ipbuf), "IP: %s", info.ip);
            sprite_draw_text(10, 82, ipbuf, FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
        }
    }
    else if (page >= 4)
    {
        // Post-provision / pairing step
        if (freehome_is_linked())
        {
            sprite_draw_text(10, 36, "FreeHome linked!", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
            const char *dev = freehome_get_device_id();
            sprite_draw_text(10, 56, dev && dev[0] ? dev : "(device id)", FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
        }
        else
        {
            sprite_draw_text(10, 36, "Ready to pair with FreeHome", FONT_MEDIUM, COLOR_BLACK, COLOR_BGROUND);
            sprite_draw_text(10, 56, "Ensure internet connection is active.", FONT_SMALL_BOLD, COLOR_BLACK, COLOR_BGROUND);
        }
    }

    // Draw Back / Next buttons at bottom
    int by = SPRITE_HEIGHT - 18;
    // Back
    uint16_t back_bg = (btn == 0) ? COLOR_WHITE : COLOR_BGROUND;
    sprite_fill_rect(10, by - 2, 64, 14, back_bg);
    sprite_draw_text(18, by, "Back", FONT_MEDIUM, COLOR_BLACK, back_bg);
    // Next
    uint16_t next_bg = (btn == 1) ? COLOR_WHITE : COLOR_BGROUND;
    sprite_fill_rect(SPRITE_WIDTH - 74, by - 2, 64, 14, next_bg);

    // If we're on the QR page and waiting for a client to connect, replace
    // the Next label with a small animated spinner so the user knows to
    // connect their phone to the AP. If a client connected, draw Next.
    if (page == 2 && !client_connected)
    {
        // Draw spinner in the Next button area
        static int phase = 0;
        phase = (phase + 1) % 8;
        const int sx = SPRITE_WIDTH - 74 + 18;
        const int sy = by + 6;
        const int radius = 3;
        // Clear spinner area
        sprite_fill_rect(SPRITE_WIDTH - 74 + 8, by - 1, 48, 12, next_bg);
        for (int i = 0; i < 8; ++i)
        {
            float ang = (2.0f * 3.14159265f * (i + phase)) / 8.0f;
            int dx = static_cast<int>(cosf(ang) * 8.0f);
            int dy = static_cast<int>(sinf(ang) * 3.0f);
            int x = sx + dx;
            int y = sy + dy;
            uint16_t c = (i == 0) ? COLOR_WHITE : COLOR_BLACK;
            sprite_fill_circle(x, y, radius, c);
        }
        // If client connected during this frame, advance the UI state now
        if (client_connected)
        {
            ui_controller_set_freehome_page(3);
        }
    }
    else
    {
        sprite_draw_text(SPRITE_WIDTH - 66, by, "Next", FONT_MEDIUM, COLOR_BLACK, next_bg);
    }
}

void display_update_progress(int current, int total)
{
    if (total <= 0)
        return;

    int bar_width = DISPLAY_WIDTH - 24;
    int fill_width = (current * bar_width) / total;

    // Draw progress bar at bottom
    display_draw_rect(10, 250, DISPLAY_WIDTH - 20, 20, COLOR_WHITE);

    // Clear progress bar interior
    display_fill_rect(12, 252, bar_width, 16, COLOR_BLACK);

    // Draw filled portion
    if (fill_width > 0)
    {
        display_fill_rect(12, 252, fill_width, 16, COLOR_GREEN);
    }
}

static void display_draw_ui(void)
{
    if (!machine_is_powered())
    {
        display_clear(COLOR_BLACK);
        display_set_backlight(0);
        return;
    }
    display_set_backlight(255);

    // Background color for the UI area (matches logo background)
    uint16_t bg_color = 0xB7FF;

    // Offset to center the 188x107 UI on 240x320 screen
    // X: (240 - 188) / 2 = 26
    // Y: 5 (Top)
    int ox = 26;
    int oy = 5;

    ui_render_state_t ui_state = ui_controller_get_render_state();

    // Draw static background once (or if needed)
    // For now, we just clear the sprite to bg_color
    sprite_clear(COLOR_BGROUND);

    if (machine_is_logo_enabled() || ui_state.menu == UI_MENU_LOGO)
    {
        // Draw Logo
        // Logo is 186x90. Draw at (0, 7) relative to sprite
        sprite_draw_bitmap(0, 7, 186, 90, lg_logo);
    }
    else if (ui_state.menu == UI_MENU_WASH_SETTINGS)
    {
        draw_wash_settings(ui_state);
    }
    else if (ui_state.menu == UI_MENU_FREEHOME)
    {
        draw_freehome_menu(ui_state);
    }
    else if (ui_state.menu == UI_MENU_MACHINE_SETTINGS)
    {
        draw_machine_settings(ui_state);
    }
    else
    {
        // Draw Main UI

        // Draw Lines
        sprite_fill_rect(9, 22, 172, 1, COLOR_BLACK);
        sprite_fill_rect(9, 77, 172, 1, COLOR_BLACK);

        sprite_fill_rect(51, 78, 1, 21, COLOR_BLACK);
        sprite_fill_rect(93, 78, 1, 21, COLOR_BLACK);
        sprite_fill_rect(138, 78, 1, 21, COLOR_BLACK);

        // Draw Text
        if (!machine_is_running())
        {
            // Program Name
            int prog = machine_get_program();
            if (prog >= 0 && prog < NUM_PROGRAMS)
            {
                sprite_draw_text(10, 48, program_profile(prog).name, FONT_LARGE, COLOR_BLACK, bg_color);
            }
        }
        else
        {
            // Cycle Name
            char stage_label[32];
            machine_get_stage_label(stage_label, sizeof(stage_label));
            if (stage_label[0] == '\0')
            {
                strncpy(stage_label, "Working", sizeof(stage_label) - 1);
                stage_label[sizeof(stage_label) - 1] = '\0';
            }
            sprite_draw_text(10, 48, stage_label, FONT_LARGE, COLOR_BLACK, bg_color);

            // Door Lock Icon
            if (!machine_is_door_open())
            {
                sprite_draw_bitmap(111, 1, 70, 21, door_lock);
            }
        }

        // Icons
        sprite_draw_bitmap(9, 1, 23, 20, turbowash);

        if (machine_get_drum_light())
        {
            sprite_draw_bitmap(33, 1, 24, 20, drumlight);
        }

        sprite_draw_bitmap(98, 26, 40, 19, est_time_remaining);

        // ETA Box
        // Outline rect using fill_rect
        sprite_fill_rect(97, 25, 79, 1, COLOR_BLACK);  // Top
        sprite_fill_rect(97, 46, 79, 1, COLOR_BLACK);  // Bottom
        sprite_fill_rect(97, 25, 1, 22, COLOR_BLACK);  // Left
        sprite_fill_rect(175, 25, 1, 22, COLOR_BLACK); // Right

        // ETA Text
        int eta = machine_get_eta();
        char buf[16];
        if (!machine_is_eta_available())
        {
            snprintf(buf, sizeof(buf), "--:--");
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d:%02d", eta / 60, eta % 60);
        }
        sprite_draw_text(141, 29, buf, FONT_MEDIUM, COLOR_BLACK, bg_color);

        // Progress Bar (Draw directly to screen or sprite? Sprite is better)
        // But progress bar is at Y=250, which is OUTSIDE the sprite (107 height + 5 offset = 112)
        // So we must draw progress bar directly to screen.
    }

    // Push sprite to screen
    sprite_push(ox, oy);

    // Draw Progress Bar (Directly to screen as it is outside sprite area)
    if (!machine_is_logo_enabled() && ui_state.menu == UI_MENU_DEFAULT)
    {
        int total = machine_get_total_stages();
        if (total <= 0)
        {
            total = NUM_CYCLES;
        }
        int current = machine_get_stage();
        display_update_progress(current, total);
    }
}

void display_task_entry(void *pvParameters)
{
    // Initial clear
    display_clear(COLOR_BLACK);

    while (1)
    {
        display_draw_ui();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Legacy functions removed/replaced by display_task_entry
void display_draw_main_screen(void) {}
void display_update_rpm(int rpm) {}
void display_update_eta(int seconds) {}
void display_update_program(const char *program_name) {}
void display_show_message(const char *message, bool is_error) {}
void display_draw_power_off(void) {}
void display_draw_door_warning(void) {}
