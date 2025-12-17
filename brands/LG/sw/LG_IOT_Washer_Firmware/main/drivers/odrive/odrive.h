/*
 * odrive.h
 * ODrive motor controller UART communication
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

#define ODRIVE_UART_NUM         UART_NUM_2
#ifndef ODRIVE_BAUD_RATE
#define ODRIVE_BAUD_RATE        1000000
#endif
#define ODRIVE_TX_PIN           17
#define ODRIVE_RX_PIN           16
#define ODRIVE_BUF_SIZE         256

/*===========================================================================
 * ODrive States
 *===========================================================================*/

typedef enum {
    AXIS_STATE_UNDEFINED = 0,
    AXIS_STATE_IDLE = 1,
    AXIS_STATE_STARTUP_SEQUENCE = 2,
    AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3,
    AXIS_STATE_MOTOR_CALIBRATION = 4,
    AXIS_STATE_ENCODER_INDEX_SEARCH = 6,
    AXIS_STATE_ENCODER_OFFSET_CALIBRATION = 7,
    AXIS_STATE_CLOSED_LOOP_CONTROL = 8,
    AXIS_STATE_LOCKIN_SPIN = 9,
    AXIS_STATE_ENCODER_DIR_FIND = 10,
    AXIS_STATE_HOMING = 11,
    AXIS_STATE_ENCODER_HALL_POLARITY_CALIBRATION = 12,
    AXIS_STATE_ENCODER_HALL_PHASE_CALIBRATION = 13,
} odrive_axis_state_t;

typedef enum {
    CONTROL_MODE_VOLTAGE_CONTROL = 0,
    CONTROL_MODE_TORQUE_CONTROL = 1,
    CONTROL_MODE_VELOCITY_CONTROL = 2,
    CONTROL_MODE_POSITION_CONTROL = 3,
} odrive_control_mode_t;

/*===========================================================================
 * API
 *===========================================================================*/

/**
 * @brief Initialize ODrive UART communication
 * @return ESP_OK on success
 */
esp_err_t odrive_init(void);

/**
 * @brief Set axis state
 * @param axis Axis number (0 or 1)
 * @param state Desired state
 * @return ESP_OK on success
 */
esp_err_t odrive_set_state(uint8_t axis, odrive_axis_state_t state);

/**
 * @brief Set control mode
 * @param axis Axis number
 * @param mode Control mode
 * @return ESP_OK on success
 */
esp_err_t odrive_set_control_mode(uint8_t axis, odrive_control_mode_t mode);

/**
 * @brief Set velocity (turns per second)
 * @param axis Axis number
 * @param velocity Velocity in turns/second (negative for reverse)
 * @return ESP_OK on success
 */
esp_err_t odrive_set_velocity(uint8_t axis, float velocity);

/**
 * @brief Set velocity with feedforward torque
 * @param axis Axis number
 * @param velocity Velocity in turns/second
 * @param torque_ff Feedforward torque
 * @return ESP_OK on success
 */
esp_err_t odrive_set_velocity_ff(uint8_t axis, float velocity, float torque_ff);

/**
 * @brief Get current velocity
 * @param axis Axis number
 * @param[out] velocity Current velocity in turns/second
 * @return ESP_OK on success
 */
esp_err_t odrive_get_velocity(uint8_t axis, float *velocity);

/**
 * @brief Get motor current
 * @param axis Axis number
 * @param[out] current Motor current in Amps
 * @return ESP_OK on success
 */
esp_err_t odrive_get_current(uint8_t axis, float *current);

/**
 * @brief Get bus voltage
 * @param[out] voltage Bus voltage in Volts
 * @return ESP_OK on success
 */
esp_err_t odrive_get_bus_voltage(float *voltage);

/**
 * @brief Emergency stop
 * @return ESP_OK on success
 */
esp_err_t odrive_emergency_stop(void);

/**
 * @brief Clear errors
 * @param axis Axis number
 * @return ESP_OK on success
 */
esp_err_t odrive_clear_errors(uint8_t axis);

/**
 * @brief Check if ODrive is connected and responding
 * @return true if connected
 */
bool odrive_is_connected(void);

/**
 * @brief Convert RPM to turns per second
 * @param rpm RPM value
 * @return Turns per second
 */
static inline float rpm_to_turns_per_sec(int rpm)
{
    return (float)rpm / 60.0f;
}

/**
 * @brief Convert turns per second to RPM
 * @param tps Turns per second
 * @return RPM
 */
static inline int turns_per_sec_to_rpm(float tps)
{
    return (int)(tps * 60.0f);
}

#ifdef __cplusplus
}
#endif
