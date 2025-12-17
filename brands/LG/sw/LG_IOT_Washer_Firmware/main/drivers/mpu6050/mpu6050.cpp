/*
 * mpu6050.c
 * MPU6050 IMU driver for balance detection
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#
/*
 * Why this driver exists and design notes:
 * - The MPU6050 is used to detect imbalance via short vibration sampling
 *   windows. This file centralises I2C handling, scaling, and a small
 *   vibration analysis buffer so the rest of the system can request a
 *   high-level `analyze_vibration` result rather than dealing with raw
 *   registers.
 * - The driver chooses moderate filtering (DLPF) and sample rates to
 *   balance sensitivity and noise; these values are intentionally tunable
 *   for different drum/wash loads and should be adjusted based on
 *   empirical measurements.
 */

#include "mpu6050.h"

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "mpu6050";

/*===========================================================================
 * MPU6050 Register Definitions
 *===========================================================================*/

#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_FIFO_EN         0x23
#define MPU6050_REG_INT_PIN_CFG     0x37
#define MPU6050_REG_INT_ENABLE      0x38
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_ACCEL_XOUT_L    0x3C
#define MPU6050_REG_ACCEL_YOUT_H    0x3D
#define MPU6050_REG_ACCEL_YOUT_L    0x3E
#define MPU6050_REG_ACCEL_ZOUT_H    0x3F
#define MPU6050_REG_ACCEL_ZOUT_L    0x40
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_TEMP_OUT_L      0x42
#define MPU6050_REG_GYRO_XOUT_H     0x43
#define MPU6050_REG_GYRO_XOUT_L     0x44
#define MPU6050_REG_GYRO_YOUT_H     0x45
#define MPU6050_REG_GYRO_YOUT_L     0x46
#define MPU6050_REG_GYRO_ZOUT_H     0x47
#define MPU6050_REG_GYRO_ZOUT_L     0x48
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_PWR_MGMT_2      0x6C
#define MPU6050_REG_WHO_AM_I        0x75

/*===========================================================================
 * State Variables
 *===========================================================================*/

static bool s_initialized = false;
static float s_accel_scale = 16384.0f;  // Default ±2g
static float s_gyro_scale = 131.0f;     // Default ±250°/s

static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static i2c_master_dev_handle_t s_mpu_device = nullptr;

static constexpr int I2C_TIMEOUT_MS = 100;

// Calibration offsets
static int16_t s_accel_offset_x = 0;
static int16_t s_accel_offset_y = 0;
static int16_t s_accel_offset_z = 0;
static int16_t s_gyro_offset_x = 0;
static int16_t s_gyro_offset_y = 0;
static int16_t s_gyro_offset_z = 0;

// Vibration analysis
#define VIBRATION_SAMPLES   50
#define IMBALANCE_THRESHOLD 2.0f  // g

static float s_accel_history_x[VIBRATION_SAMPLES];
static float s_accel_history_y[VIBRATION_SAMPLES];
static float s_accel_history_z[VIBRATION_SAMPLES];
static int s_history_index = 0;

/*===========================================================================
 * Internal I2C Functions
 *===========================================================================*/

static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t data)
{
    if (!s_mpu_device) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[2] = { reg, data };
    return i2c_master_transmit(s_mpu_device, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t mpu6050_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    if (!s_mpu_device) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_transmit_receive(
        s_mpu_device,
        &reg,
        1,
        data,
        len,
        I2C_TIMEOUT_MS);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

esp_err_t mpu6050_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    
    if (!s_i2c_bus) {
        esp_err_t ret = i2c_master_get_bus_handle(MPU6050_I2C_NUM, &s_i2c_bus);
        if (ret != ESP_OK) {
            i2c_master_bus_config_t bus_cfg = {};
            bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT,
            bus_cfg.i2c_port = MPU6050_I2C_NUM,
            bus_cfg.sda_io_num = (gpio_num_t)MPU6050_I2C_SDA,
            bus_cfg.scl_io_num = (gpio_num_t)MPU6050_I2C_SCL,
            bus_cfg.glitch_ignore_cnt = 7,
            bus_cfg.flags.enable_internal_pullup = true;

            ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
                return ret;
            }
        }
    }

    if (!s_mpu_device) {
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = MPU6050_I2C_ADDR;
        dev_cfg.scl_speed_hz = MPU6050_I2C_FREQ;

        esp_err_t ret = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_mpu_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add MPU6050 device: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Check device ID
    uint8_t who_am_i;
    esp_err_t ret = mpu6050_get_device_id(&who_am_i);
    if (ret != ESP_OK || who_am_i != 0x68) {
        ESP_LOGE(TAG, "MPU6050 not found (WHO_AM_I = 0x%02X)", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Wake up device (clear sleep bit)
    ret = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for device to stabilize
    
    // Configure sample rate divider (1 kHz / (1 + 9) = 100 Hz)
    mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 9);
    
    // Configure DLPF (20 Hz bandwidth)
    mpu6050_write_reg(MPU6050_REG_CONFIG, 4);
    
    // Configure accelerometer (±4g for washing machine vibration)
    mpu6050_set_accel_range(1);
    
    // Configure gyroscope (±500°/s)
    mpu6050_set_gyro_range(1);
    
    // Initialize history buffers
    memset(s_accel_history_x, 0, sizeof(s_accel_history_x));
    memset(s_accel_history_y, 0, sizeof(s_accel_history_y));
    memset(s_accel_history_z, 0, sizeof(s_accel_history_z));
    
    s_initialized = true;
    ESP_LOGI(TAG, "MPU6050 initialized");
    
    return ESP_OK;
}

esp_err_t mpu6050_read_raw(mpu6050_raw_data_t *data)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read 14 bytes starting from ACCEL_XOUT_H
    uint8_t buf[14];
    esp_err_t ret = mpu6050_read_reg(MPU6050_REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Parse data (big-endian)
    data->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    data->temp    = (int16_t)((buf[6] << 8) | buf[7]);
    data->gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);
    
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    mpu6050_raw_data_t raw;
    esp_err_t ret = mpu6050_read_raw(&raw);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Apply calibration offsets and convert
    data->accel_x_g = (float)(raw.accel_x - s_accel_offset_x) / s_accel_scale;
    data->accel_y_g = (float)(raw.accel_y - s_accel_offset_y) / s_accel_scale;
    data->accel_z_g = (float)(raw.accel_z - s_accel_offset_z) / s_accel_scale;
    
    data->gyro_x_dps = (float)(raw.gyro_x - s_gyro_offset_x) / s_gyro_scale;
    data->gyro_y_dps = (float)(raw.gyro_y - s_gyro_offset_y) / s_gyro_scale;
    data->gyro_z_dps = (float)(raw.gyro_z - s_gyro_offset_z) / s_gyro_scale;
    
    // Temperature: (raw / 340) + 36.53
    data->temp_c = (float)raw.temp / 340.0f + 36.53f;
    
    return ESP_OK;
}

esp_err_t mpu6050_analyze_vibration(mpu6050_vibration_t *vibration)
{
    mpu6050_data_t data;
    esp_err_t ret = mpu6050_read(&data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Store in history buffer
    s_accel_history_x[s_history_index] = data.accel_x_g;
    s_accel_history_y[s_history_index] = data.accel_y_g;
    s_accel_history_z[s_history_index] = data.accel_z_g;
    s_history_index = (s_history_index + 1) % VIBRATION_SAMPLES;
    
    // Calculate variance (vibration magnitude) for each axis
    float sum_x = 0, sum_y = 0, sum_z = 0;
    float sum_sq_x = 0, sum_sq_y = 0, sum_sq_z = 0;
    
    for (int i = 0; i < VIBRATION_SAMPLES; i++) {
        sum_x += s_accel_history_x[i];
        sum_y += s_accel_history_y[i];
        sum_z += s_accel_history_z[i];
        sum_sq_x += s_accel_history_x[i] * s_accel_history_x[i];
        sum_sq_y += s_accel_history_y[i] * s_accel_history_y[i];
        sum_sq_z += s_accel_history_z[i] * s_accel_history_z[i];
    }
    
    float mean_x = sum_x / VIBRATION_SAMPLES;
    float mean_y = sum_y / VIBRATION_SAMPLES;
    float mean_z = sum_z / VIBRATION_SAMPLES;
    
    float var_x = (sum_sq_x / VIBRATION_SAMPLES) - (mean_x * mean_x);
    float var_y = (sum_sq_y / VIBRATION_SAMPLES) - (mean_y * mean_y);
    float var_z = (sum_sq_z / VIBRATION_SAMPLES) - (mean_z * mean_z);
    
    // RMS vibration magnitude
    vibration->magnitude = sqrtf(var_x + var_y + var_z);
    
    // Find dominant axis
    if (var_x > var_y && var_x > var_z) {
        vibration->dominant_axis = 0;  // X
    } else if (var_y > var_z) {
        vibration->dominant_axis = 1;  // Y
    } else {
        vibration->dominant_axis = 2;  // Z
    }
    
    // Check imbalance threshold
    vibration->imbalanced = (vibration->magnitude > IMBALANCE_THRESHOLD);
    
    return ESP_OK;
}

esp_err_t mpu6050_set_accel_range(uint8_t range)
{
    if (range > 3) range = 3;
    
    esp_err_t ret = mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, range << 3);
    if (ret == ESP_OK) {
        // Update scale factor
        switch (range) {
            case 0: s_accel_scale = 16384.0f; break;  // ±2g
            case 1: s_accel_scale = 8192.0f;  break;  // ±4g
            case 2: s_accel_scale = 4096.0f;  break;  // ±8g
            case 3: s_accel_scale = 2048.0f;  break;  // ±16g
        }
    }
    return ret;
}

esp_err_t mpu6050_set_gyro_range(uint8_t range)
{
    if (range > 3) range = 3;
    
    esp_err_t ret = mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, range << 3);
    if (ret == ESP_OK) {
        // Update scale factor
        switch (range) {
            case 0: s_gyro_scale = 131.0f;  break;  // ±250°/s
            case 1: s_gyro_scale = 65.5f;   break;  // ±500°/s
            case 2: s_gyro_scale = 32.8f;   break;  // ±1000°/s
            case 3: s_gyro_scale = 16.4f;   break;  // ±2000°/s
        }
    }
    return ret;
}

esp_err_t mpu6050_set_dlpf(uint8_t dlpf)
{
    if (dlpf > 6) dlpf = 6;
    return mpu6050_write_reg(MPU6050_REG_CONFIG, dlpf);
}

esp_err_t mpu6050_calibrate(void)
{
    ESP_LOGI(TAG, "Calibrating MPU6050 (keep device stationary)...");
    
    int32_t accel_sum_x = 0, accel_sum_y = 0, accel_sum_z = 0;
    int32_t gyro_sum_x = 0, gyro_sum_y = 0, gyro_sum_z = 0;
    const int num_samples = 100;
    
    for (int i = 0; i < num_samples; i++) {
        mpu6050_raw_data_t raw;
        if (mpu6050_read_raw(&raw) == ESP_OK) {
            accel_sum_x += raw.accel_x;
            accel_sum_y += raw.accel_y;
            accel_sum_z += raw.accel_z;
            gyro_sum_x += raw.gyro_x;
            gyro_sum_y += raw.gyro_y;
            gyro_sum_z += raw.gyro_z;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    s_accel_offset_x = accel_sum_x / num_samples;
    s_accel_offset_y = accel_sum_y / num_samples;
    s_accel_offset_z = (accel_sum_z / num_samples) - (int16_t)s_accel_scale;  // Subtract 1g
    s_gyro_offset_x = gyro_sum_x / num_samples;
    s_gyro_offset_y = gyro_sum_y / num_samples;
    s_gyro_offset_z = gyro_sum_z / num_samples;
    
    ESP_LOGI(TAG, "Calibration complete. Offsets: accel(%d,%d,%d) gyro(%d,%d,%d)",
             s_accel_offset_x, s_accel_offset_y, s_accel_offset_z,
             s_gyro_offset_x, s_gyro_offset_y, s_gyro_offset_z);
    
    return ESP_OK;
}

bool mpu6050_is_connected(void)
{
    uint8_t id;
    if (mpu6050_get_device_id(&id) == ESP_OK && id == 0x68) {
        return true;
    }
    return false;
}

esp_err_t mpu6050_get_device_id(uint8_t *id)
{
    return mpu6050_read_reg(MPU6050_REG_WHO_AM_I, id, 1);
}
