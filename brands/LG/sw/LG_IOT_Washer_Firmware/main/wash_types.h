/*
 * wash_types.h
 * Definitions for wash cycle parameters and types
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WASH_ACTION_TUMBLING = 0,
    WASH_ACTION_ROLLING,
    WASH_ACTION_FILTRATION,
    WASH_ACTION_SWINGING,
    WASH_ACTION_STEPPING,
    WASH_ACTION_SCRUBBING,
} wash_action_t;

#define MAX_WASH_ACTIONS 4

typedef struct {
    uint8_t count;
    wash_action_t actions[MAX_WASH_ACTIONS];
} wash_action_list_t;

typedef enum {
    WASH_SECTION_DETECTING = 0,
    WASH_SECTION_SATURATION,
    WASH_SECTION_PREWASH,
    WASH_SECTION_MAINWASH,
    WASH_SECTION_INTERIM_SPIN,
    WASH_SECTION_RINSE,
    WASH_SECTION_FINAL_SPIN,
} wash_section_kind_t;

typedef struct {
    // Pre-action
    bool fill_water;
    bool drain_water;
    
    // Motion (Tumble)
    int tumble_rpm;
    int tumble_duration_ms;
    int stop_duration_ms;
    float pump_on_start_frac; // 0.0 to 1.0
    float pump_on_end_frac;   // 0.0 to 1.0
    bool alternate_direction;
    
    // Pumps
    int circulation_pump_pwm;
    int pump_on_steps;
    int pump_on_step_ms;
    
    // Spin (overrides tumble if > 0)
    int spin_rpm;
} wash_params_t;

#ifdef __cplusplus
}
#endif
