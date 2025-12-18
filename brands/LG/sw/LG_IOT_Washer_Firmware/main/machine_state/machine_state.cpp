/*
 * machine_state.cpp
 * Global machine state variables and initialization
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Why this module holds global state:
 * - Centralising machine state and observer notifications simplifies
 *   reasoning about concurrency and ensures a single authoritative source
 *   of truth for UI, tasks and actuators. Access is protected by a mutex
 *   to provide deterministic updates and allow snapshotting for
 *   display/telemetry without races.
 * - Observers are intentionally simple function pointers to minimise
 *   runtime dependencies; if the project grows consider an event bus or
 *   a more flexible subscription mechanism.
 */

#include "machine_state.h"
#include "constants.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#if CONFIG_SIMULATOR_MODE
#include "simulator.h"
#endif

static const char *TAG = "machine_state";

/*===========================================================================
 * Private State Storage
 *===========================================================================*/

static motor_state_t motor_state;
static program_state_t program_state;
static system_state_t system_state;
static machine_state_observer_t observers[4] = {0};

static SemaphoreHandle_t state_mutex = nullptr;

#if CONFIG_SIMULATOR_MODE
static inline void snapshot_motor_state(int *target_rpm, float *current_rpm, bool *direction_ccw)
{
    if (!target_rpm || !current_rpm || !direction_ccw) {
        return;
    }
    *target_rpm = motor_state.target_rpm;
    *current_rpm = motor_state.current_rpm;
    *direction_ccw = motor_state.direction_ccw;
}
#endif

/*===========================================================================
 * Initialization
 *===========================================================================*/

esp_err_t machine_state_init(void)
{
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_FAIL;
    }

    // Initialize Motor State
    motor_state.target_rpm = 0;
    motor_state.current_rpm = 0.0f;
    motor_state.direction_ccw = false;
    motor_state.enabled = true;
    motor_state.pwm_value = 0;

    // Initialize Program State
    program_state.program_id = 0;
    program_state.current_stage = 0;
    program_state.is_running = false; // Default to stopped
    program_state.is_powered = false;
    program_state.eta_seconds = 0;
    program_state.elapsed_seconds = 0;
    program_state.eta_available = false;
    program_state.prewash_enabled = false;
    program_state.extra_rinse_count = 0;
    program_state.total_stages = 0;
    program_state.stage_label[0] = '\0';
    program_state.temp_idx = 0;
    program_state.spin_idx = 0;
    program_state.soil_idx = 0;
    program_state.load_size = 0;

    // Initialize System State
    system_state.door_open = false;
    system_state.drain_pump_on = false;
    system_state.fill_pump_on = false;
    system_state.drum_light_on = false;
    system_state.muted = false;
    system_state.power_led_on = false;
    system_state.start_stop_led_on = false;
    system_state.logo_enabled = false;

    ESP_LOGI(TAG, "Machine state initialized");
    return ESP_OK;
}

/*===========================================================================
 * Helper Macros
 *===========================================================================*/

#define LOCK_STATE()                              \
    if (state_mutex == nullptr) {                    \
        return;                                  \
    }                                             \
    xSemaphoreTake(state_mutex, portMAX_DELAY)

#define UNLOCK_STATE() \
    xSemaphoreGive(state_mutex)
#define LOCK_OBSERVERS() xSemaphoreTake(state_mutex, portMAX_DELAY)
#define UNLOCK_OBSERVERS() xSemaphoreGive(state_mutex)

static void notify_observers(void)
{
    machine_observable_state_t snapshot;
    LOCK_STATE();
    snapshot.powered = program_state.is_powered;
    snapshot.running = program_state.is_running;
    snapshot.door_open = system_state.door_open;
    snapshot.stage = program_state.current_stage;
    snapshot.total_stages = program_state.total_stages;
    strncpy(snapshot.stage_label, program_state.stage_label, sizeof(snapshot.stage_label) - 1);
    snapshot.stage_label[sizeof(snapshot.stage_label) - 1] = '\0';
    snapshot.eta_seconds = program_state.eta_seconds;
    snapshot.eta_available = program_state.eta_available;
    snapshot.target_rpm = motor_state.target_rpm;
    snapshot.current_rpm = motor_state.current_rpm;
    snapshot.direction_ccw = motor_state.direction_ccw;
    UNLOCK_STATE();

    for (size_t i = 0; i < sizeof(observers) / sizeof(observers[0]); ++i) {
        if (observers[i]) {
            observers[i](&snapshot);
        }
    }
}

bool machine_register_observer(machine_state_observer_t cb)
{
    if (!cb || state_mutex == nullptr) {
        return false;
    }
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    for (size_t i = 0; i < sizeof(observers) / sizeof(observers[0]); ++i) {
        if (observers[i] == cb) {
            xSemaphoreGive(state_mutex);
            return true; // already registered
        }
        if (!observers[i]) {
            observers[i] = cb;
            xSemaphoreGive(state_mutex);
            return true;
        }
    }
    xSemaphoreGive(state_mutex);
    return false; // no space
}

void machine_unregister_observer(machine_state_observer_t cb)
{
    if (!cb) return;
    LOCK_STATE();
    for (size_t i = 0; i < sizeof(observers) / sizeof(observers[0]); ++i) {
        if (observers[i] == cb) {
            observers[i] = nullptr;
        }
    }
    UNLOCK_STATE();
}

void machine_get_observable_state(machine_observable_state_t *out_state)
{
    if (!out_state) return;
    LOCK_STATE();
    out_state->powered = program_state.is_powered;
    out_state->running = program_state.is_running;
    out_state->door_open = system_state.door_open;
    out_state->stage = program_state.current_stage;
    out_state->total_stages = program_state.total_stages;
    strncpy(out_state->stage_label, program_state.stage_label, sizeof(out_state->stage_label) - 1);
    out_state->stage_label[sizeof(out_state->stage_label) - 1] = '\0';
    out_state->eta_seconds = program_state.eta_seconds;
    out_state->eta_available = program_state.eta_available;
    out_state->target_rpm = motor_state.target_rpm;
    out_state->current_rpm = motor_state.current_rpm;
    out_state->direction_ccw = motor_state.direction_ccw;
    UNLOCK_STATE();
}

#define LOCK_STATE_RET(ret_val)                   \
    if (state_mutex == nullptr) {                    \
        return ret_val;                           \
    }                                             \
    xSemaphoreTake(state_mutex, portMAX_DELAY)

/*===========================================================================
 * Motor State Accessors
 *===========================================================================*/

void machine_set_target_rpm(int rpm) {
#if CONFIG_SIMULATOR_MODE
    int snapshot_target = 0;
    float snapshot_current = 0.0f;
    bool snapshot_dir = false;
#endif
    LOCK_STATE();
    motor_state.target_rpm = rpm;
#if CONFIG_SIMULATOR_MODE
    snapshot_motor_state(&snapshot_target, &snapshot_current, &snapshot_dir);
#endif
    UNLOCK_STATE();
    notify_observers();
#if CONFIG_SIMULATOR_MODE
    simulator_send_motor_state(snapshot_target, snapshot_current, snapshot_dir);
#endif
}

int machine_get_target_rpm(void) {
    LOCK_STATE_RET(0);
    int val = motor_state.target_rpm;
    UNLOCK_STATE();
    return val;
}

void machine_set_current_rpm(float rpm) {
#if CONFIG_SIMULATOR_MODE
    int snapshot_target = 0;
    float snapshot_current = 0.0f;
    bool snapshot_dir = false;
#endif
    LOCK_STATE();
    motor_state.current_rpm = rpm;
#if CONFIG_SIMULATOR_MODE
    snapshot_motor_state(&snapshot_target, &snapshot_current, &snapshot_dir);
#endif
    UNLOCK_STATE();
    notify_observers();
#if CONFIG_SIMULATOR_MODE
    simulator_send_motor_state(snapshot_target, snapshot_current, snapshot_dir);
#endif
}

float machine_get_current_rpm(void) {
    LOCK_STATE_RET(0.0f);
    float val = motor_state.current_rpm;
    UNLOCK_STATE();
    return val;
}

void machine_set_motor_dir(bool ccw) {
#if CONFIG_SIMULATOR_MODE
    int snapshot_target = 0;
    float snapshot_current = 0.0f;
    bool snapshot_dir = false;
#endif
    LOCK_STATE();
    motor_state.direction_ccw = ccw;
#if CONFIG_SIMULATOR_MODE
    snapshot_motor_state(&snapshot_target, &snapshot_current, &snapshot_dir);
#endif
    UNLOCK_STATE();
    notify_observers();
#if CONFIG_SIMULATOR_MODE
    simulator_send_motor_state(snapshot_target, snapshot_current, snapshot_dir);
#endif
}

bool machine_get_motor_dir(void) {
    LOCK_STATE_RET(false);
    bool val = motor_state.direction_ccw;
    UNLOCK_STATE();
    return val;
}

void machine_set_pwm(int pwm) {
    LOCK_STATE();
    motor_state.pwm_value = pwm;
    UNLOCK_STATE();
}

int machine_get_pwm(void) {
    LOCK_STATE_RET(0);
    int val = motor_state.pwm_value;
    UNLOCK_STATE();
    return val;
}

/*===========================================================================
 * Program State Accessors
 *===========================================================================*/

void machine_set_program(int program_id) {
    if (program_id < 0 || program_id >= NUM_PROGRAMS) {
        return;
    }
    LOCK_STATE();
    if (program_state.program_id != program_id) {
        program_state.program_id = program_id;
        const ProgramProfile &profile = program_profile(program_id);
        program_state.temp_idx = profile.default_temp_idx;
        program_state.spin_idx = profile.default_spin_idx;
        program_state.soil_idx = profile.default_soil_idx;
        UNLOCK_STATE();
        notify_observers();
    } else {
        UNLOCK_STATE();
    }
}

int machine_get_program(void) {
    LOCK_STATE_RET(0);
    int val = program_state.program_id;
    UNLOCK_STATE();
    return val;
}

void machine_set_stage(int stage) {
    LOCK_STATE();
    program_state.current_stage = stage;
    UNLOCK_STATE();
    notify_observers();
}

int machine_get_stage(void) {
    LOCK_STATE_RET(0);
    int val = program_state.current_stage;
    UNLOCK_STATE();
    return val;
}

void machine_set_running(bool running) {
    LOCK_STATE();
    program_state.is_running = running;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_running(void) {
    LOCK_STATE_RET(false);
    bool val = program_state.is_running;
    UNLOCK_STATE();
    return val;
}

void machine_set_powered(bool powered) {
    LOCK_STATE();
    program_state.is_powered = powered;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_powered(void) {
    LOCK_STATE_RET(false);
    bool val = program_state.is_powered;
    UNLOCK_STATE();
    return val;
}

void machine_set_eta(int seconds) {
    LOCK_STATE();
    program_state.eta_seconds = seconds;
    UNLOCK_STATE();
    notify_observers();
}

int machine_get_eta(void) {
    LOCK_STATE_RET(0);
    int val = program_state.eta_seconds;
    UNLOCK_STATE();
    return val;
}

void machine_increment_elapsed(void) {
    LOCK_STATE();
    program_state.elapsed_seconds++;
    UNLOCK_STATE();
}

void machine_set_elapsed_seconds(int seconds) {
    LOCK_STATE();
    program_state.elapsed_seconds = seconds;
    UNLOCK_STATE();
    notify_observers();
}

void machine_set_eta_available(bool available) {
    LOCK_STATE();
    program_state.eta_available = available;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_eta_available(void) {
    LOCK_STATE_RET(false);
    bool val = program_state.eta_available;
    UNLOCK_STATE();
    return val;
}

void machine_set_prewash_enabled(bool enabled) {
    LOCK_STATE();
    program_state.prewash_enabled = enabled;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_prewash_enabled(void) {
    LOCK_STATE_RET(false);
    bool val = program_state.prewash_enabled;
    UNLOCK_STATE();
    return val;
}

void machine_set_extra_rinse_count(uint8_t count) {
    if (count > 3) {
        count = 3;
    }
    LOCK_STATE();
    program_state.extra_rinse_count = count;
    UNLOCK_STATE();
    notify_observers();
}

uint8_t machine_get_extra_rinse_count(void) {
    LOCK_STATE_RET(0);
    uint8_t val = program_state.extra_rinse_count;
    UNLOCK_STATE();
    return val;
}

void machine_set_total_stages(int total) {
    LOCK_STATE();
    program_state.total_stages = total;
    UNLOCK_STATE();
    notify_observers();
}

int machine_get_total_stages(void) {
    LOCK_STATE_RET(0);
    int val = program_state.total_stages;
    UNLOCK_STATE();
    return val;
}

void machine_set_stage_label(const char *label) {
    if (!label) {
        label = "";
    }
    LOCK_STATE();
    strncpy(program_state.stage_label, label, sizeof(program_state.stage_label) - 1);
    program_state.stage_label[sizeof(program_state.stage_label) - 1] = '\0';
    UNLOCK_STATE();
    notify_observers();
}

void machine_get_stage_label(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return;
    }
    LOCK_STATE();
    strncpy(buffer, program_state.stage_label, buffer_len - 1);
    buffer[buffer_len - 1] = '\0';
    UNLOCK_STATE();
}

static int clamp_idx(int idx, int min_idx, int max_idx) {
    if (idx < min_idx) return min_idx;
    if (idx > max_idx) return max_idx;
    return idx;
}

void machine_set_temp_idx(int idx) {
    LOCK_STATE();
    const ProgramProfile &profile = program_profile(program_state.program_id);
    int min_idx = profile.default_temp_idx < 0 ? 0 : profile.default_temp_idx;
    int max_idx = profile.max_temp_idx < 0 ? min_idx : profile.max_temp_idx;
    program_state.temp_idx = clamp_idx(idx, min_idx, max_idx);
    UNLOCK_STATE();
}

int machine_get_temp_idx(void) {
    LOCK_STATE_RET(0);
    int val = program_state.temp_idx;
    UNLOCK_STATE();
    return val;
}

void machine_set_spin_idx(int idx) {
    LOCK_STATE();
    const ProgramProfile &profile = program_profile(program_state.program_id);
    int min_idx = profile.default_spin_idx < 0 ? 0 : profile.default_spin_idx;
    int max_idx = profile.max_spin_idx < 0 ? min_idx : profile.max_spin_idx;
    program_state.spin_idx = clamp_idx(idx, min_idx, max_idx);
    UNLOCK_STATE();
}

int machine_get_spin_idx(void) {
    LOCK_STATE_RET(0);
    int val = program_state.spin_idx;
    UNLOCK_STATE();
    return val;
}

void machine_set_soil_idx(int idx) {
    LOCK_STATE();
    const ProgramProfile &profile = program_profile(program_state.program_id);
    int min_idx = profile.default_soil_idx < 0 ? 0 : profile.default_soil_idx;
    int max_idx = profile.max_soil_idx < 0 ? min_idx : profile.max_soil_idx;
    program_state.soil_idx = clamp_idx(idx, min_idx, max_idx);
    UNLOCK_STATE();
}

int machine_get_soil_idx(void) {
    LOCK_STATE_RET(0);
    int val = program_state.soil_idx;
    UNLOCK_STATE();
    return val;
}

void machine_set_load_size(int size) {
    LOCK_STATE();
    if (size < 0) size = 0;
    if (size >= NUM_LOAD_SIZES) size = NUM_LOAD_SIZES - 1;
    program_state.load_size = size;
    UNLOCK_STATE();
}

int machine_get_load_size(void) {
    LOCK_STATE_RET(0);
    int val = program_state.load_size;
    UNLOCK_STATE();
    return val;
}

/*===========================================================================
 * System State Accessors
 *===========================================================================*/

void machine_set_door_open(bool open) {
    LOCK_STATE();
    system_state.door_open = open;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_door_open(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.door_open;
    UNLOCK_STATE();
    return val;
}

void machine_set_drain(bool on) {
    LOCK_STATE();
    system_state.drain_pump_on = on;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_get_drain(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.drain_pump_on;
    UNLOCK_STATE();
    return val;
}

void machine_set_fill(bool on) {
    LOCK_STATE();
    system_state.fill_pump_on = on;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_get_fill(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.fill_pump_on;
    UNLOCK_STATE();
    return val;
}

void machine_set_drum_light(bool on) {
    LOCK_STATE();
    system_state.drum_light_on = on;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_get_drum_light(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.drum_light_on;
    UNLOCK_STATE();
    return val;
}

void machine_set_muted(bool muted) {
    LOCK_STATE();
    system_state.muted = muted;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_muted(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.muted;
    UNLOCK_STATE();
    return val;
}

void machine_set_power_led(bool on) {
    LOCK_STATE();
    system_state.power_led_on = on;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_get_power_led(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.power_led_on;
    UNLOCK_STATE();
    return val;
}

void machine_set_start_stop_led(bool on) {
    LOCK_STATE();
    system_state.start_stop_led_on = on;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_get_start_stop_led(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.start_stop_led_on;
    UNLOCK_STATE();
    return val;
}

void machine_set_logo_enabled(bool enabled) {
    LOCK_STATE();
    system_state.logo_enabled = enabled;
    UNLOCK_STATE();
    notify_observers();
}

bool machine_is_logo_enabled(void) {
    LOCK_STATE_RET(false);
    bool val = system_state.logo_enabled;
    UNLOCK_STATE();
    return val;
}
