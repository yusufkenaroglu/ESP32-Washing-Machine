/*
 * wash_plan.h
 * Plan construction utilities for wash programs
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "wash_types.h"

#define MAX_WASH_SECTIONS 16
#define BASE_RINSE_COUNT 2
#define MAX_TOTAL_RINSES 5

typedef struct {
    wash_section_kind_t kind;
    int32_t remaining_seconds;
    char label[24];
    wash_params_t params;
    wash_action_list_t actions;
} wash_section_instance_t;

typedef struct {
    wash_section_instance_t sections[MAX_WASH_SECTIONS];
    size_t length;
} wash_plan_t;

wash_params_t wash_defaults_for_section(wash_section_kind_t kind, int program);
bool wash_plan_build(wash_plan_t *plan, int program, int load_size, bool prewash_enabled, uint8_t extra_rinses);
int wash_plan_eta_from(const wash_plan_t *plan, size_t start_index);
