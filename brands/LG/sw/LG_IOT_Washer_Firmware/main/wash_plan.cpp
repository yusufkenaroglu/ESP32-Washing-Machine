/*
 * wash_plan.cpp
 * Plan construction utilities for wash programs
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#
/*
 * Why this module exists:
 * - Centralises the logic for constructing wash plans so tuning durations,
 *   default parameters, and action lists is done in one place. This keeps
 *   higher-level control code (tasks) focused on execution rather than
 *   policy.
 * - The helpers here (clamping, time conversion, append_section) are small
 *   and intentionally conservative to make it easy to test plan construction
 *   independently of the runtime.
 */

#include "wash_plan.h"

#include <algorithm>
#include <cstdio>
#include "constants.h"

namespace {

int clamp_seconds(int seconds, int min_seconds, int max_seconds) {
    if (seconds < min_seconds) {
        return min_seconds;
    }
    if (seconds > max_seconds) {
        return max_seconds;
    }
    return seconds;
}

void append_section(wash_plan_t *plan, wash_section_kind_t kind, const char *label, int32_t seconds, const wash_params_t &params, const wash_action_list_t *actions) {
    if (!plan || seconds <= 0) {
        return;
    }
    if (plan->length >= MAX_WASH_SECTIONS) {
        return;
    }
    wash_section_instance_t &slot = plan->sections[plan->length++];
    slot.kind = kind;
    slot.remaining_seconds = seconds;
    slot.params = params;
    if (actions) {
        slot.actions = *actions;
    } else {
        slot.actions.count = 0;
    }
    std::snprintf(slot.label, sizeof(slot.label), "%s", label ? label : "");
}
} // namespace

wash_params_t wash_defaults_for_section(wash_section_kind_t kind, int program) {
    wash_params_t p = {};
    p.tumble_rpm = 60;
    p.tumble_duration_ms = 10000;
    p.stop_duration_ms = 2000;
    p.pump_on_start_frac = 0.0f;
    p.pump_on_end_frac = 1.0f;
    p.alternate_direction = true;
    p.circulation_pump_pwm = 4095;

    switch (kind) {
        case WASH_SECTION_DETECTING:
            p.tumble_rpm = 40;
            p.tumble_duration_ms = 2000;
            p.stop_duration_ms = 1000;
            p.circulation_pump_pwm = 0;
            break;
        case WASH_SECTION_SATURATION:
            p.fill_water = true;
            p.tumble_rpm = 50;
            p.tumble_duration_ms = 15000;
            break;
        case WASH_SECTION_PREWASH:
        case WASH_SECTION_MAINWASH:
            p.tumble_rpm = 60;
            p.pump_on_start_frac = 0.2f;
            p.pump_on_end_frac = 0.8f;
            (void)program;
            break;
        case WASH_SECTION_INTERIM_SPIN:
            p.drain_water = true;
            p.spin_rpm = 400;
            break;
        case WASH_SECTION_RINSE:
            p.fill_water = true;
            p.tumble_rpm = 55;
            break;
        case WASH_SECTION_FINAL_SPIN:
            p.drain_water = true;
            p.spin_rpm = 1000;
            break;
    }
    return p;
}

bool wash_plan_build(wash_plan_t *plan, int program, int load_size, bool prewash_enabled, uint8_t extra_rinses) {
    if (!plan) {
        return false;
    }
    plan->length = 0;
    const int main_wash_seconds = clamp_seconds(/* seconds_from_minutes(profile.tumble_min) */3000, 300, 3600);
    const int saturation_seconds = clamp_seconds(/*seconds_from_minutes(profile.stop_min)*/400, 120, 900);
    const uint8_t total_rinses = std::min<uint8_t>(BASE_RINSE_COUNT + extra_rinses, MAX_TOTAL_RINSES);
    const int interim_spin_seconds = 90;
    const int rinse_seconds = 240;
    const int final_spin_seconds = 360;

    const wash_action_list_t &actions = program_actions_for_load(program, load_size);

    append_section(plan, WASH_SECTION_DETECTING, "Detecting", 90, wash_defaults_for_section(WASH_SECTION_DETECTING, program), nullptr);
    append_section(plan, WASH_SECTION_SATURATION, "Saturation", saturation_seconds, wash_defaults_for_section(WASH_SECTION_SATURATION, program), &actions);

    if (prewash_enabled) {
        int prewash_seconds = clamp_seconds(main_wash_seconds / 3, 180, 900);
        append_section(plan, WASH_SECTION_PREWASH, "Pre-wash", prewash_seconds, wash_defaults_for_section(WASH_SECTION_PREWASH, program), &actions);
    }

    append_section(plan, WASH_SECTION_MAINWASH, "Main wash", main_wash_seconds, wash_defaults_for_section(WASH_SECTION_MAINWASH, program), &actions);
    append_section(plan, WASH_SECTION_INTERIM_SPIN, "Interim spin", interim_spin_seconds, wash_defaults_for_section(WASH_SECTION_INTERIM_SPIN, program), nullptr);

    for (uint8_t rinse_index = 0; rinse_index < total_rinses; ++rinse_index) {
        char label[24];
        std::snprintf(label, sizeof(label), "Rinse %u", rinse_index + 1);
        append_section(plan, WASH_SECTION_RINSE, label, rinse_seconds, wash_defaults_for_section(WASH_SECTION_RINSE, program), &actions);
        if (rinse_index < total_rinses - 1) {
            append_section(plan, WASH_SECTION_INTERIM_SPIN, "Interim spin", interim_spin_seconds / 2, wash_defaults_for_section(WASH_SECTION_INTERIM_SPIN, program), nullptr);
        }
    }

    append_section(plan, WASH_SECTION_FINAL_SPIN, "Final spin", final_spin_seconds, wash_defaults_for_section(WASH_SECTION_FINAL_SPIN, program), nullptr);

    return plan->length > 0;
}

int wash_plan_eta_from(const wash_plan_t *plan, size_t start_index) {
    if (!plan || start_index >= plan->length) {
        return 0;
    }
    int eta = 0;
    for (size_t i = start_index; i < plan->length; ++i) {
        eta += plan->sections[i].remaining_seconds;
    }
    return eta;
}
