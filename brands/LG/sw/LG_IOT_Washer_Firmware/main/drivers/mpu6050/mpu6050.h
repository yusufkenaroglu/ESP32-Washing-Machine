/*
 * mpu6050.h
 * MPU6050 IMU driver for balance detection
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
 * Configuration
 *===========================================================================*/

#define MPU6050_I2C_NUM         I2C_NUM_0
#define MPU6050_I2C_SDA         21
#define MPU6050_I2C_SCL         22
#define MPU6050_I2C_FREQ        400000  // 400 kHz
#define MPU6050_I2C_ADDR        0x68    // AD0 pin low

/*===========================================================================
 * Data Structures
 *===========================================================================*/

typedef struct {
    int16_t accel_x;    // Raw accelerometer X
    int16_t accel_y;    // Raw accelerometer Y
    int16_t accel_z;    // Raw accelerometer Z
    int16_t gyro_x;     // Raw gyroscope X
    int16_t gyro_y;     // Raw gyroscope Y
    int16_t gyro_z;     // Raw gyroscope Z
    int16_t temp;       // Raw temperature
} mpu6050_raw_data_t;

typedef struct {
    float accel_x_g;    // Acceleration X in g
    float accel_y_g;    // Acceleration Y in g
    float accel_z_g;    // Acceleration Z in g
    float gyro_x_dps;   // Angular velocity X in degrees/sec
    float gyro_y_dps;   // Angular velocity Y in degrees/sec
    float gyro_z_dps;   // Angular velocity Z in degrees/sec
    float temp_c;       // Temperature in Celsius
} mpu6050_data_t;

typedef struct {
    float magnitude;    // Overall vibration magnitude
    float dominant_axis;// Which axis has most vibration (0=X, 1=Y, 2=Z)
    bool imbalanced;    // True if imbalance detected
} mpu6050_vibration_t;

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief Initialize MPU6050
 * @return ESP_OK on success
 */
esp_err_t mpu6050_init(void);

/**
 * @brief Read raw sensor data
 * @param[out] data Raw data structure
 * @return ESP_OK on success
 */
esp_err_t mpu6050_read_raw(mpu6050_raw_data_t *data);

/**
 * @brief Read and convert sensor data
 * @param[out] data Converted data structure
 * @return ESP_OK on success
 */
esp_err_t mpu6050_read(mpu6050_data_t *data);

/**
 * @brief Analyze vibration for balance detection
 * @param[out] vibration Vibration analysis result
 * @return ESP_OK on success
 */
esp_err_t mpu6050_analyze_vibration(mpu6050_vibration_t *vibration);

/**
 * @brief Set accelerometer full-scale range
 * @param range 0=±2g, 1=±4g, 2=±8g, 3=±16g
 * @return ESP_OK on success
 */
esp_err_t mpu6050_set_accel_range(uint8_t range);

/**
 * @brief Set gyroscope full-scale range
 * @param range 0=±250°/s, 1=±500°/s, 2=±1000°/s, 3=±2000°/s
 * @return ESP_OK on success
 */
esp_err_t mpu6050_set_gyro_range(uint8_t range);

/**
 * @brief Set digital low-pass filter
 * @param dlpf DLPF setting (0-6)
 * @return ESP_OK on success
 */
esp_err_t mpu6050_set_dlpf(uint8_t dlpf);

/**
 * @brief Calibrate sensor (call when stationary)
 * @return ESP_OK on success
 */
esp_err_t mpu6050_calibrate(void);

/**
 * @brief Check if MPU6050 is connected
 * @return true if connected
 */
bool mpu6050_is_connected(void);

/**
 * @brief Get device ID (should be 0x68)
 * @param[out] id Device ID
 * @return ESP_OK on success
 */
esp_err_t mpu6050_get_device_id(uint8_t *id);

#ifdef __cplusplus
}
#endif
