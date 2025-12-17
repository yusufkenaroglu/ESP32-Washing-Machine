/*
 * display.h
 * TFT display driver interface
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Display Configuration
 *===========================================================================*/

// TFT display pins (ST7789 or ILI9341 compatible)
#define DISPLAY_SPI_HOST    SPI2_HOST
#define DISPLAY_PIN_MOSI    GPIO_NUM_23
#define DISPLAY_PIN_SCLK    GPIO_NUM_18
#define DISPLAY_PIN_CS      GPIO_NUM_5
#define DISPLAY_PIN_DC      GPIO_NUM_2
#define DISPLAY_PIN_RST     GPIO_NUM_4
#define DISPLAY_PIN_BL      GPIO_NUM_19      // Backlight

// Display dimensions
#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH       240
#endif
#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT      320
#endif

/*===========================================================================
 * Color Definitions (RGB565)
 *===========================================================================*/

#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_ORANGE        0xFC00
#define COLOR_GRAY          0x8410
#define COLOR_DARK_GRAY     0x4208

// LG brand colors
#define COLOR_LG_RED        0xA800
#define COLOR_LG_GRAY       0x7BEF

#define COLOR_BGROUND       0xB7FF

/*===========================================================================
 * Data Structures
 *===========================================================================*/

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} display_rect_t;

typedef enum {
    FONT_SMALL_BOLD = 0,
    FONT_MEDIUM = 1,
    FONT_LARGE = 2,
    FONT_XLARGE = 3,
    FONT_XXLARGE = 4,
} display_font_t;

typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTRE = 1,
    ALIGN_RIGHT = 2,
} display_align_t;

/*===========================================================================
 * Display API
 *===========================================================================*/

/**
 * @brief Initialize display
 * @return ESP_OK on success
 */
esp_err_t display_init(void);

/**
 * @brief Clear entire display with color
 * @param color RGB565 color
 */
void display_clear(uint16_t color);

/**
 * @brief Fill rectangle with color
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param color RGB565 color
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Draw rectangle outline
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param color RGB565 color
 */
void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Draw circle
 * @param x Center X
 * @param y Center Y
 * @param r Radius
 * @param color RGB565 color
 */
void display_draw_circle(int16_t x, int16_t y, int16_t r, uint16_t color);

/**
 * @brief Fill circle
 * @param x Center X
 * @param y Center Y
 * @param r Radius
 * @param color RGB565 color
 */
void display_fill_circle(int16_t x, int16_t y, int16_t r, uint16_t color);

/**
 * @brief Draw line
 * @param x0 Start X
 * @param y0 Start Y
 * @param x1 End X
 * @param y1 End Y
 * @param color RGB565 color
 */
void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/**
 * @brief Set pixel
 * @param x X position
 * @param y Y position
 * @param color RGB565 color
 */
void display_set_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Draw text
 * @param x X position
 * @param y Y position
 * @param text Text string
 * @param font Font size
 * @param fg_color Foreground color
 * @param bg_color Background color
 */
void display_draw_text(int16_t x, int16_t y, const char *text, 
                        display_font_t font, uint16_t fg_color, uint16_t bg_color);

/**
 * @brief Draw text with alignment
 * @param rect Bounding rectangle
 * @param text Text string
 * @param font Font size
 * @param align Alignment
 * @param fg_color Foreground color
 * @param bg_color Background color
 */
void display_draw_text_aligned(display_rect_t rect, const char *text,
                                display_font_t font, display_align_t align,
                                uint16_t fg_color, uint16_t bg_color);

/**
 * @brief Draw bitmap/image
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param data Pixel data (RGB565)
 */
void display_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, 
                          const uint16_t *data);

/**
 * @brief Draw monochrome bitmap
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param data Monochrome bitmap data
 * @param fg_color Foreground color
 * @param bg_color Background color
 */
void display_draw_bitmap_mono(int16_t x, int16_t y, int16_t w, int16_t h,
                               const uint8_t *data, uint16_t fg_color, uint16_t bg_color);

/**
 * @brief Set backlight brightness
 * @param brightness 0-255
 */
void display_set_backlight(uint8_t brightness);

/**
 * @brief Get display width
 * @return Width in pixels
 */
uint16_t display_get_width(void);

/**
 * @brief Get display height
 * @return Height in pixels
 */
uint16_t display_get_height(void);

/*===========================================================================
 * Washing Machine UI API
 *===========================================================================*/

/**
 * @brief Draw main washing machine screen
 */
void display_draw_main_screen(void);

/**
 * @brief Update RPM display
 * @param rpm Current RPM
 */
void display_update_rpm(int rpm);

/**
 * @brief Update ETA display
 * @param seconds Seconds remaining
 */
void display_update_eta(int seconds);

/**
 * @brief Update program name
 * @param program_name Program name string
 */
void display_update_program(const char *program_name);

/**
 * @brief Update cycle progress
 * @param current Current cycle (0-based)
 * @param total Total cycles
 */
void display_update_progress(int current, int total);

/**
 * @brief Show status message
 * @param message Message string
 * @param is_error True for error styling
 */
void display_show_message(const char *message, bool is_error);

/**
 * @brief Draw power-off screen
 */
void display_draw_power_off(void);

/**
 * @brief Draw door-open warning
 */
void display_draw_door_warning(void);

/**
 * @brief Set simulator callback for drawing
 * @param cb Callback function receiving rect coordinates and color
 */
void display_set_simulator_hook(void (*cb)(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color));

/**
 * @brief Set simulator callback for drawing bitmaps
 * @param cb Callback function receiving rect coordinates and pixel data
 */
void display_set_simulator_bitmap_hook(void (*cb)(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data));

/**
 * @brief Display task entry point
 * @param pvParameters Task parameters
 */
void display_task_entry(void *pvParameters);

#ifdef __cplusplus
}
#endif
