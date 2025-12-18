/*
 * tasks.cpp
 * Event-driven control plane built on FreeRTOS primitives
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tasks.h"

#include "app_config.h"
#include "constants.h"
#include "drivers/gpio_hal/gpio_hal.h"
#include "ulp/ulp_manager.h"
#include "machine_state.h"
#include "ui_controller.h"
#include "drivers/display/display.h"
#include "drivers/sound/sound.h"
#include "wash_plan.h"
#if CONFIG_BALANCE_DETECTION
#include "drivers/mpu6050/mpu6050.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_err.h"
#include "wash_types.h"
#include "drivers/odrive/odrive.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char *TAG = "wm_control";

static QueueHandle_t s_event_queue = nullptr;
static QueueHandle_t s_command_queue = nullptr;
static TaskHandle_t s_wash_task_handle = nullptr;
static TaskHandle_t s_mgr_task = nullptr;
static TaskHandle_t s_actuator_task = nullptr;
static TaskHandle_t s_io_task = nullptr;
#if CONFIG_BALANCE_DETECTION
static TaskHandle_t s_sensor_task = nullptr;
#endif
static TaskHandle_t s_tick_task = nullptr;
static TaskHandle_t s_display_task = nullptr;

/*
 * Why this task/queue architecture:
 * - The control plane is event-driven to decouple hardware interrupts and
 *   time-critical I/O from higher-level wash logic. Queues provide natural
 *   backpressure and bounded buffering so producers don't block critical
 *   ISRs. Timeouts used when sending to queues (20-50ms) are chosen to be
 *   short so the system remains responsive while still tolerating brief
 *   contention; they are a trade-off between robustness and latency.
 * - Tasks are split by responsibility (manager, actuators, IO, display,
 *   wash motion) to improve isolation and make it easier to suspend/resume
 *   subsets during power saving or error handling.
 */

static inline bool enqueue_event_internal(wm_event_type_t type, int32_t value)
{
    if (!s_event_queue) {
        return false;
    }
    wm_event_t evt = {
        .type = type,
        .value = value,
    };
    return xQueueSend(s_event_queue, &evt, pdMS_TO_TICKS(20)) == pdTRUE;
}

static inline void enqueue_command(wm_command_type_t type, int32_t arg0, int32_t arg1)
{
    if (!s_command_queue) {
        return;
    }
    wm_command_t cmd = {
        .type = type,
        .arg0 = arg0,
        .arg1 = arg1,
    };
    xQueueSend(s_command_queue, &cmd, pdMS_TO_TICKS(50));
}

bool tasks_post_event(const wm_event_t *event)
{
    if (!event) {
        return false;
    }
    return enqueue_event_internal(event->type, event->value);
}

bool tasks_post_simple_event(wm_event_type_t type, int32_t value)
{
    return enqueue_event_internal(type, value);
}

bool tasks_post_dial_delta(int delta)
{
    return enqueue_event_internal(WM_EVENT_DIAL_DELTA, delta);
}

static void suspend_if(TaskHandle_t h) {
    if (h) vTaskSuspend(h);
}

static void resume_if(TaskHandle_t h) {
    if (h) vTaskResume(h);
}

static void delete_if(TaskHandle_t &h) {
    if (h) {
        vTaskDelete(h);
        h = nullptr;
    }
}

void tasks_suspend_all(void)
{
    suspend_if(s_mgr_task);
    suspend_if(s_actuator_task);
    suspend_if(s_io_task);
#if CONFIG_BALANCE_DETECTION
    suspend_if(s_sensor_task);
#endif
    suspend_if(s_tick_task);
    suspend_if(s_display_task);
    suspend_if(s_wash_task_handle);
}

void tasks_resume_all(void)
{
    resume_if(s_mgr_task);
    resume_if(s_actuator_task);
    resume_if(s_io_task);
#if CONFIG_BALANCE_DETECTION
    resume_if(s_sensor_task);
#endif
    resume_if(s_tick_task);
    resume_if(s_display_task);
    resume_if(s_wash_task_handle);
}

void tasks_delete_all(void)
{
    delete_if(s_mgr_task);
    delete_if(s_actuator_task);
    delete_if(s_io_task);
#if CONFIG_BALANCE_DETECTION
    delete_if(s_sensor_task);
#endif
    delete_if(s_tick_task);
    delete_if(s_display_task);
    delete_if(s_wash_task_handle);
}

struct WmRuntimeContext {
    size_t stage_index = 0;
    wash_plan_t plan = {};
};

static void wash_motion_task_entry(void *arg)
{
    wash_params_t params = *(wash_params_t *)arg;
    free(arg);
    bool dir = false;

    // Helper: bounded wait that sleeps in small chunks so the task remains
    // responsive and timing can be polled or interrupted by the scheduler.
    auto bounded_delay_ms = [](int total_ms) {
        const int chunk = 50; // 50ms chunks
        int remaining = total_ms;
        while (remaining > 0) {
            int wait = remaining > chunk ? chunk : remaining;
            vTaskDelay(pdMS_TO_TICKS(wait));
            remaining -= wait;
        }
    };

    /*
     * Start-of-wash primitive behaviour:
     * - `fill_water` uses a fixed-time fill here for simplicity and to keep
     *   the motion task self-contained. In production, this should be
     *   replaced with a sensor-driven loop (inertial regressive model fitting) so
     *   filling stops when the measured condition is satisfied. This choice
     *   keeps the initial implementation predictable for tests.
     */
    if (params.fill_water) {
        pwm_set_fill_pump(4095);
        // Use bounded wait so task stays responsive and can be preempted
        bounded_delay_ms(10000);
        pwm_set_fill_pump(0);
    }

    if (params.drain_water) {
        pwm_set_drain_pump(4095);
        // Drain continues during spin usually.
    }

    if (params.spin_rpm > 0) {
        // Spin cycle: set target velocity and then wait while allowing
        // the task to remain responsive. Use bounded_delay_ms so that
        // other events (stop/abort) can be serviced in a timely manner.
        odrive_set_velocity(0, rpm_to_turns_per_sec(params.spin_rpm));
        // Wait indefinitely until task is deleted by stop_wash_action; use
        // a long bounded loop to avoid a single huge blocking delay.
        while (true) {
            bounded_delay_ms(1000);
        }
    }

    // Tumble cycle
    while (true) {
        if (params.alternate_direction) {
            // Stop before reversing
            odrive_set_velocity(0, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
            dir = !dir;
        }

        float velocity = rpm_to_turns_per_sec(params.tumble_rpm);
        if (dir) velocity = -velocity;
        odrive_set_velocity(0, velocity);

        if (params.pump_on_steps > 0) {
            for (int i = 0; i < params.pump_on_steps; i++) {
                pwm_set_circulation_pump(params.circulation_pump_pwm);
                bounded_delay_ms(params.pump_on_step_ms);
                pwm_set_circulation_pump(0);
                bounded_delay_ms(params.pump_on_step_ms);
                if (params.alternate_direction) {
                    dir = !dir;
                    velocity = -velocity;
                    odrive_set_velocity(0, velocity);
                }
            }
            odrive_set_velocity(0, 0);
            bounded_delay_ms(params.stop_duration_ms);
            continue;
        }

        int tumble_ms = params.tumble_duration_ms;
        int pump_on_start = (int)(tumble_ms * params.pump_on_start_frac);
        int pump_on_end = (int)(tumble_ms * params.pump_on_end_frac);

        if (pump_on_start > 0) {
            bounded_delay_ms(pump_on_start);
        }

        if (pump_on_end > pump_on_start) {
            pwm_set_circulation_pump(params.circulation_pump_pwm);
            bounded_delay_ms(pump_on_end - pump_on_start);
            pwm_set_circulation_pump(0);
        }

        if (tumble_ms > pump_on_end) {
            bounded_delay_ms(tumble_ms - pump_on_end);
        }

        odrive_set_velocity(0, 0);
        vTaskDelay(pdMS_TO_TICKS(params.stop_duration_ms));
    }
}

static void start_wash_action(const wash_params_t &params)
{
    if (s_wash_task_handle) {
        vTaskDelete(s_wash_task_handle);
        s_wash_task_handle = nullptr;
    }
    /*
     * We copy `params` to heap memory and pass it to the wash task because
     * FreeRTOS task entry functions accept a `void*` parameter. Allocating a
     * small copy on the heap avoids stack lifetime issues and keeps the task
     * API simple. The caller must ensure the runtime doesn't run out of heap
     * for many concurrent wash starts; if that becomes a problem, switch to
     * a pooled allocator or pass an index into preallocated storage.
     */
    wash_params_t *p = (wash_params_t *)malloc(sizeof(wash_params_t));
    if (p) {
        *p = params;
        xTaskCreate(wash_motion_task_entry, "wash_motion", 4096, p, 4, &s_wash_task_handle);
    }
}

static void stop_wash_action(void)
{
    if (s_wash_task_handle) {
        vTaskDelete(s_wash_task_handle);
        s_wash_task_handle = nullptr;
    }
    odrive_set_velocity(0, 0);
    pwm_set_circulation_pump(0);
    pwm_set_fill_pump(0);
    pwm_set_drain_pump(0);
}

static void publish_plan_metadata(const WmRuntimeContext &ctx)
{
    machine_set_total_stages(static_cast<int>(ctx.plan.length));
    if (ctx.plan.length == 0 || ctx.stage_index >= ctx.plan.length) {
        machine_set_stage_label("");
    } else {
        machine_set_stage_label(ctx.plan.sections[ctx.stage_index].label);
    }
}

static bool rebuild_program_plan(WmRuntimeContext &ctx)
{
    ctx.stage_index = 0;
    bool ok = wash_plan_build(&ctx.plan, machine_get_program(), machine_get_load_size(), machine_is_prewash_enabled(), machine_get_extra_rinse_count());
    if (!ok) {
        ESP_LOGE(TAG, "Wash plan is empty");
        machine_set_eta_available(false);
        machine_set_eta(0);
        return false;
    }

    machine_set_stage(0);
    machine_set_eta_available(true);
    machine_set_eta(wash_plan_eta_from(&ctx.plan, 0));
    publish_plan_metadata(ctx);
    return true;
}

static void apply_power_on(WmRuntimeContext &ctx)
{
    if (machine_is_powered()) {
        return;
    }
    machine_set_powered(true);
    machine_set_running(false);
    machine_set_eta(0);
    machine_set_elapsed_seconds(0);
    machine_set_logo_enabled(true);
    ui_controller_show_logo();
    machine_set_drum_light(false);
    enqueue_command(WM_CMD_SET_POWER_LED, 1, 0);
    enqueue_command(WM_CMD_SET_DRUM_LED, 0, 0);
    enqueue_command(WM_CMD_SET_START_LED, 0, 0);
    enqueue_command(WM_CMD_SET_LOGO_ENABLE, 1, 0);
    if (!machine_is_muted()) {
        enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_ON, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (int level = 0; level <= 3072; level += 64) {
        enqueue_command(WM_CMD_SET_DRUM_LED, level, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    machine_set_drum_light(true);
    enqueue_command(WM_CMD_SET_LOGO_ENABLE, 0, 0);
    machine_set_logo_enabled(false);
    ui_controller_reset();
    ESP_LOGI(TAG, "Power on sequence complete");
    // Enable both power and start buttons while the machine is on
    ulp_set_button_mask(0x3);

}

static void apply_power_off(WmRuntimeContext &ctx)
{
    if (!machine_is_powered()) {
        return;
    }
    machine_set_running(false);
    machine_set_powered(false);
    machine_set_eta(0);
    machine_set_elapsed_seconds(0);
    machine_set_stage(0);
    machine_set_total_stages(0);
    machine_set_stage_label("");
    machine_set_eta_available(false);
    machine_set_logo_enabled(true);
    ui_controller_show_logo();
    enqueue_command(WM_CMD_SET_START_LED, 0, 0);
    enqueue_command(WM_CMD_SET_LOGO_ENABLE, 1, 0);
    if (!machine_is_muted()) {
        enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_OFF, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (int level = 3072; level >= 0; level -= 64) {
        enqueue_command(WM_CMD_SET_DRUM_LED, level, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    machine_set_drum_light(false);
    enqueue_command(WM_CMD_SET_POWER_LED, 0, 0);
    enqueue_command(WM_CMD_SET_LOGO_ENABLE, 0, 0);
    ESP_LOGI(TAG, "Power off sequence complete");
    // Only the power button should be active while off
    ulp_set_button_mask(0x1);
    ESP_LOGI(TAG, "Power off: entering deep sleep with ULP watching power button");
    ESP_ERROR_CHECK(ulp_power_enter_deep_sleep());
}

static void complete_cycle(WmRuntimeContext &ctx)
{
    machine_set_running(false);
    machine_set_eta(0);
    machine_set_eta_available(false);
    machine_set_stage(static_cast<int>(ctx.plan.length));
    machine_set_stage_label("Complete");
    enqueue_command(WM_CMD_SET_START_LED, 0, 0);
    if (!machine_is_muted()) {
        enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_CYCLE_END, 0);
    }
    stop_wash_action();
    ESP_LOGI(TAG, "Cycle complete");
}

static void start_cycle(WmRuntimeContext &ctx)
{
    if (!machine_is_powered()) {
        ESP_LOGW(TAG, "Ignoring start request while powered off");
        return;
    }
    if (machine_is_running()) {
        return;
    }
    if (machine_is_door_open()) {
        ESP_LOGW(TAG, "Door is open, refusing to start");
        if (!machine_is_muted()) {
            enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_ERROR, 0);
        }
        return;
    }
    if (!rebuild_program_plan(ctx)) {
        ESP_LOGE(TAG, "Failed to build wash plan; aborting start");
        return;
    }
    machine_set_running(true);
    enqueue_command(WM_CMD_SET_START_LED, 1, 0);
    if (!machine_is_muted()) {
        enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_CYCLE_START, 0);
    }
    start_wash_action(ctx.plan.sections[ctx.stage_index].params);
    ESP_LOGI(TAG, "Cycle started");
}

static void pause_cycle(WmRuntimeContext &ctx)
{
    if (!machine_is_running()) {
        return;
    }
    machine_set_running(false);
    enqueue_command(WM_CMD_SET_START_LED, 0, 0);
    if (!machine_is_muted()) {
        enqueue_command(WM_CMD_PLAY_SOUND, SOUND_EFFECT_STOP, 0);
    }
    stop_wash_action();
    ESP_LOGI(TAG, "Cycle paused");
}

static void process_timer_tick(WmRuntimeContext &ctx)
{
    if (!machine_is_powered() || !machine_is_running()) {
        return;
    }
    if (ctx.plan.length == 0 || ctx.stage_index >= ctx.plan.length) {
        return;
    }
    wash_section_instance_t &section = ctx.plan.sections[ctx.stage_index];
    if (section.remaining_seconds > 0) {
        section.remaining_seconds -= 1;
        machine_increment_elapsed();
    }
    machine_set_eta(wash_plan_eta_from(&ctx.plan, ctx.stage_index));
    if (section.remaining_seconds <= 0) {
        ctx.stage_index++;
        machine_set_stage(static_cast<int>(ctx.stage_index));
        publish_plan_metadata(ctx);
        if (ctx.stage_index >= ctx.plan.length) {
            complete_cycle(ctx);
        } else {
            ESP_LOGI(TAG, "Advancing to %s", ctx.plan.sections[ctx.stage_index].label);
            start_wash_action(ctx.plan.sections[ctx.stage_index].params);
        }
    }
}

static void system_manager_task(void *arg)
{
    (void)arg;
    WmRuntimeContext ctx;
    ESP_LOGI(TAG, "System manager started");
    while (true) {
        wm_event_t evt;
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (evt.type) {
            case WM_EVENT_POWER_BUTTON:
                if (machine_is_powered()) {
                    apply_power_off(ctx);
                } else {
                    apply_power_on(ctx);
                }
                break;
            case WM_EVENT_START_BUTTON:
                if (ui_controller_handle_start_press()) {
                    break;
                }
                if (machine_is_running()) {
                    pause_cycle(ctx);
                } else {
                    start_cycle(ctx);
                }
                break;
            case WM_EVENT_START_LONG_PRESS:
                ui_controller_handle_start_long_press();
                break;
            case WM_EVENT_DOOR_STATE: {
                const bool door_open = (evt.value != 0);
                machine_set_door_open(door_open);
                ESP_LOGI(TAG, "Door state: %s", door_open ? "open" : "closed");
                if (door_open && machine_is_running()) {
                    pause_cycle(ctx);
                }
                break;
            }
            case WM_EVENT_TIMER_TICK:
                process_timer_tick(ctx);
                break;
            case WM_EVENT_SENSOR_SAMPLE:
                ESP_LOGW(TAG, "Imbalance detected, magnitude=%ld", evt.value);
                break;
            case WM_EVENT_DIAL_DELTA:
                ui_controller_handle_dial_delta(evt.value);
                break;
            default:
                break;
        }
    }
}

static void actuator_task(void *arg)
{
    (void)arg;
    wm_command_t cmd;
    while (true) {
        if (xQueueReceive(s_command_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (cmd.type) {
            case WM_CMD_SET_POWER_LED:
                gpio_write(PIN_POWER_LED, cmd.arg0 ? 1 : 0);
                machine_set_power_led(cmd.arg0 != 0);
                break;
            case WM_CMD_SET_DRUM_LED:
                pwm_set_drum_led(cmd.arg0);
                break;
            case WM_CMD_SET_START_LED:
                gpio_write(PIN_START_STOP_LED, cmd.arg0 ? 1 : 0);
                machine_set_start_stop_led(cmd.arg0 != 0);
                break;
            case WM_CMD_PLAY_SOUND:
                sound_play_effect((uint8_t)cmd.arg0);
                break;
            case WM_CMD_SET_LOGO_ENABLE:
                machine_set_logo_enabled(cmd.arg0 != 0);
                break;
            case WM_CMD_SET_CIRC_PUMP_PWM:
                pwm_set_circulation_pump(cmd.arg0);
                break;
            case WM_CMD_SET_FILL_PUMP_PWM:
                pwm_set_fill_pump(cmd.arg0);
                break;
            case WM_CMD_SET_DRAIN_PUMP_PWM:
                pwm_set_drain_pump(cmd.arg0);
                break;
            default:
                break;
        }
    }
}

static void io_scanner_task(void *arg)
{
    (void)arg;
    bool last_door = machine_is_door_open();
    enqueue_event_internal(WM_EVENT_DOOR_STATE, last_door ? 1 : 0);
    while (true) {
        check_buttons();
        bool door_now = machine_is_door_open();
        if (door_now != last_door) {
            last_door = door_now;
            enqueue_event_internal(WM_EVENT_DOOR_STATE, door_now ? 1 : 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#if CONFIG_BALANCE_DETECTION
static void sensor_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(500);
    while (true) {
        mpu6050_vibration_t vibe;
        if (mpu6050_analyze_vibration(&vibe) == ESP_OK) {
            if (vibe.imbalanced) {
                int32_t magnitude_milli_g = (int32_t)(vibe.magnitude * 1000.0f);
                enqueue_event_internal(WM_EVENT_SENSOR_SAMPLE, magnitude_milli_g);
            }
        }
        vTaskDelay(period);
    }
}
#endif

static void timer_tick_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(1000);
    TickType_t last = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last, period);
        enqueue_event_internal(WM_EVENT_TIMER_TICK, 0);
    }
}

esp_err_t tasks_create_all(void)
{
    ui_controller_reset();

    s_event_queue = xQueueCreate(32, sizeof(wm_event_t));
    s_command_queue = xQueueCreate(32, sizeof(wm_command_t));
    if (!s_event_queue || !s_command_queue) {
        ESP_LOGE(TAG, "Failed to create control queues");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret;
    ret = xTaskCreatePinnedToCore(system_manager_task, "wm_mgr", 6144, nullptr, 6, &s_mgr_task, 1);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }
    ret = xTaskCreatePinnedToCore(actuator_task, "wm_act", 4096, nullptr, 5, &s_actuator_task, 1);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }
    ret = xTaskCreatePinnedToCore(io_scanner_task, "wm_io", 3072, nullptr, 4, &s_io_task, 0);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }
#if CONFIG_BALANCE_DETECTION
    ret = xTaskCreatePinnedToCore(sensor_task, "wm_sensor", 4096, nullptr, 3, &s_sensor_task, 0);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }
#endif
    ret = xTaskCreatePinnedToCore(timer_tick_task, "wm_tick", 2048, nullptr, 2, &s_tick_task, 0);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }
    ret = xTaskCreatePinnedToCore(display_task_entry, "wm_display", 4096, nullptr, 2, &s_display_task, 1);
    if (ret != pdPASS) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Control plane tasks created");
    return ESP_OK;
}
