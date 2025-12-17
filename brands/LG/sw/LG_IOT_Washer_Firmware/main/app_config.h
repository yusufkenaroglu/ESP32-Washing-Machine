/*
 * app_config.h
 * Hardware configuration and feature flags
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Feature Flags (via Kconfig/sdkconfig)
 *===========================================================================*/
/* Feature toggles are provided by Kconfig and available from sdkconfig.h.
 * Do not define CONFIG_* flags here to avoid divergence between build
 * configuration and source-level assumptions. See main/Kconfig. */
#include "sdkconfig.h"

/*===========================================================================
 * GPIO Pin Definitions
 *===========================================================================*/

// PWM Outputs (LEDC channels)
#define PIN_CIRCULATION_PUMP    GPIO_NUM_15
#define PIN_DRAIN_PUMP          GPIO_NUM_27
#define PIN_DRUM_LED            GPIO_NUM_13
#define PIN_FILL_PUMP           GPIO_NUM_12

// Digital Outputs
#define PIN_START_STOP_LED      GPIO_NUM_14
#define PIN_POWER_LED           GPIO_NUM_26

// DAC Output (Sound)
#define PIN_SPEAKER             GPIO_NUM_25

// Digital Inputs
#define PIN_POWER_BUTTON        GPIO_NUM_33
#define PIN_START_STOP_BUTTON   GPIO_NUM_32
#define PIN_DOOR_SENSOR         GPIO_NUM_34

// UART for ODrive
#define PIN_ODRIVE_TX           GPIO_NUM_17
#define PIN_ODRIVE_RX           GPIO_NUM_16
#define ODRIVE_UART_NUM         UART_NUM_2
#define ODRIVE_BAUD_RATE        4800

// I2C for MPU6050
#define PIN_I2C_SDA             GPIO_NUM_21
#define PIN_I2C_SCL             GPIO_NUM_22
#define I2C_PORT_NUM            I2C_NUM_0
#define I2C_FREQ_HZ             400000
#define MPU6050_ADDR            0x68

// SPI for TFT Display (ST7789)
#define PIN_TFT_MOSI            GPIO_NUM_23
#define PIN_TFT_SCLK            GPIO_NUM_18
#define PIN_TFT_CS              GPIO_NUM_5
#define PIN_TFT_DC              GPIO_NUM_2
#define PIN_TFT_RST             GPIO_NUM_4
#define PIN_TFT_BL              GPIO_NUM_19
#define TFT_SPI_HOST            VSPI_HOST
#define TFT_SPI_FREQ            20000000

/*===========================================================================
 * Display Configuration
 *===========================================================================*/
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define SPRITE_WIDTH            188
#define SPRITE_HEIGHT           107

/*===========================================================================
 * LEDC (PWM) Channel Configuration
 *===========================================================================*/
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_12_BIT   // 12-bit resolution (0-4095)
#define LEDC_FREQUENCY          19500               // 19.5 kHz PWM frequency

// Channel assignments
#define LEDC_CH_CIRCULATION     LEDC_CHANNEL_0
#define LEDC_CH_DRAIN           LEDC_CHANNEL_1
#define LEDC_CH_DRUM_LED        LEDC_CHANNEL_2
#define LEDC_CH_FILL            LEDC_CHANNEL_3

/*===========================================================================
 * WiFi Configuration
 *===========================================================================*/
#define WIFI_MAX_RETRY          5
#define OTA_HOSTNAME            "LG_OTA"

/*===========================================================================
 * Timing Constants (milliseconds/seconds)
 *===========================================================================*/
#define BUTTON_DEBOUNCE_MS      50
#define DISPLAY_REFRESH_MS      100
#define MOTOR_UPDATE_MS         50

#ifdef __cplusplus
}
#endif
