/*
 * machine_state.h
 * Global state variables for the washing machine
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * State Structures
 *===========================================================================*/

typedef struct {
    int target_rpm;
    float current_rpm;
    bool direction_ccw; // true=CCW, false=CW
    bool enabled;
    int pwm_value;
} motor_state_t;

typedef struct {
    int program_id;
    int current_stage; // turns
    bool is_running;   // !program_stopped
    bool is_powered;
    int eta_seconds;
    int elapsed_seconds;
    bool eta_available;
    bool prewash_enabled;
    uint8_t extra_rinse_count;
    int total_stages;
    char stage_label[32];
    int temp_idx;
    int spin_idx;
    int soil_idx;
    int load_size;
} program_state_t;

typedef struct {
    bool door_open;
    bool drain_pump_on;
    bool fill_pump_on;
    bool drum_light_on;
    bool muted;
    bool power_led_on;
    bool start_stop_led_on;
    bool logo_enabled;
} system_state_t;

typedef struct {
    bool powered;
    bool running;
    bool door_open;
    int stage;
    int total_stages;
    char stage_label[32];
    int eta_seconds;
    bool eta_available;
    int target_rpm;
    float current_rpm;
    bool direction_ccw;
} machine_observable_state_t;

typedef void (*machine_state_observer_t)(const machine_observable_state_t *state);

/*===========================================================================
 * Global State Accessors (Thread-Safe)
 *===========================================================================*/

// Initialization
esp_err_t machine_state_init(void);

// Motor State
void machine_set_target_rpm(int rpm);
int machine_get_target_rpm(void);
void machine_set_current_rpm(float rpm);
float machine_get_current_rpm(void);
void machine_set_motor_dir(bool ccw);
bool machine_get_motor_dir(void);
void machine_set_pwm(int pwm);
int machine_get_pwm(void);

// Program State
void machine_set_program(int program_id);
int machine_get_program(void);
void machine_set_stage(int stage);
int machine_get_stage(void);
void machine_set_running(bool running);
bool machine_is_running(void);
void machine_set_powered(bool powered);
bool machine_is_powered(void);
void machine_set_eta(int seconds);
int machine_get_eta(void);
void machine_increment_elapsed(void);
void machine_set_elapsed_seconds(int seconds);
void machine_set_eta_available(bool available);
bool machine_is_eta_available(void);
void machine_set_prewash_enabled(bool enabled);
bool machine_is_prewash_enabled(void);
void machine_set_extra_rinse_count(uint8_t count);
uint8_t machine_get_extra_rinse_count(void);
void machine_set_temp_idx(int idx);
int machine_get_temp_idx(void);
void machine_set_spin_idx(int idx);
int machine_get_spin_idx(void);
void machine_set_soil_idx(int idx);
int machine_get_soil_idx(void);
void machine_set_load_size(int size);
int machine_get_load_size(void);
void machine_set_total_stages(int total);
int machine_get_total_stages(void);
void machine_set_stage_label(const char *label);
void machine_get_stage_label(char *buffer, size_t buffer_len);

// System State
void machine_set_door_open(bool open);
bool machine_is_door_open(void);
void machine_set_drain(bool on);
bool machine_get_drain(void);
void machine_set_fill(bool on);
bool machine_get_fill(void);
void machine_set_drum_light(bool on);
bool machine_get_drum_light(void);
void machine_set_muted(bool muted);
bool machine_is_muted(void);
void machine_set_power_led(bool on);
bool machine_get_power_led(void);
void machine_set_start_stop_led(bool on);
bool machine_get_start_stop_led(void);
void machine_set_logo_enabled(bool enabled);
bool machine_is_logo_enabled(void);

// Observers
bool machine_register_observer(machine_state_observer_t cb);
void machine_unregister_observer(machine_state_observer_t cb);
void machine_get_observable_state(machine_observable_state_t *out_state);

#ifdef __cplusplus
}
#endif
