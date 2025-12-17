/*
 * tasks.h
 * Real-time control plane APIs and event definitions
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WM_EVENT_POWER_BUTTON = 0,
    WM_EVENT_START_BUTTON = 1,
    WM_EVENT_DOOR_STATE   = 2,
    WM_EVENT_TIMER_TICK   = 3,
    WM_EVENT_SENSOR_SAMPLE = 4,
    WM_EVENT_START_LONG_PRESS = 5,
    WM_EVENT_DIAL_DELTA = 6,
} wm_event_type_t;

typedef struct {
    wm_event_type_t type;
    int32_t value;
} wm_event_t;

typedef enum {
    WM_CMD_SET_POWER_LED = 0,
    WM_CMD_SET_DRUM_LED,
    WM_CMD_SET_START_LED,
    WM_CMD_PLAY_SOUND,
    WM_CMD_SET_LOGO_ENABLE,
    WM_CMD_SET_CIRC_PUMP_PWM,
    WM_CMD_SET_FILL_PUMP_PWM,
    WM_CMD_SET_DRAIN_PUMP_PWM,
} wm_command_type_t;

typedef struct {
    wm_command_type_t type;
    int32_t arg0;
    int32_t arg1;
} wm_command_t;

esp_err_t tasks_create_all(void);
bool tasks_post_event(const wm_event_t *event);
bool tasks_post_simple_event(wm_event_type_t type, int32_t value);
bool tasks_post_dial_delta(int delta);
void tasks_suspend_all(void);
void tasks_resume_all(void);
void tasks_delete_all(void);

#ifdef __cplusplus
}
#endif
