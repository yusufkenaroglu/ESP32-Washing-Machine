/*
 * constants.h
 * Program timing presets, statistical parameters, and program names
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "wash_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Program Timing Arrays
 *===========================================================================*/
#define NUM_PROGRAMS    14
#define NUM_CYCLES      8
#define NUM_LOAD_SIZES  3

typedef struct {
	const char *name;
	uint8_t tumble_min;
	uint8_t stop_min;
	int8_t default_temp_idx;
    int8_t min_temp_idx;
	int8_t max_temp_idx;
	int8_t default_spin_idx;
    int8_t min_spin_idx;
	int8_t max_spin_idx;
    int8_t default_soil_idx;
    int8_t min_soil_idx;
    int8_t max_soil_idx;
} ProgramProfile;

typedef struct {
    wash_action_list_t loads[NUM_LOAD_SIZES];
} ProgramActionProfile;

extern const ProgramProfile program_profiles[NUM_PROGRAMS];
extern const ProgramActionProfile program_actions[NUM_PROGRAMS];

// Temperature presets
extern const char *const temperatures[6];

// Spin speed presets
extern const char *const spin_speeds[6];

// Soil level presets
extern const char *const soil_levels[4];


/*===========================================================================
 * Program and Cycle Names
 *===========================================================================*/
extern const char *const cycle_names[NUM_CYCLES];

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
inline const ProgramProfile &program_profile(int idx) { return program_profiles[idx]; }
inline const ProgramActionProfile &program_action_profile(int idx) { return program_actions[idx]; }
inline const wash_action_list_t &program_actions_for_load(int program, int load_size) {
	if (program < 0 || program >= NUM_PROGRAMS) {
		static const wash_action_list_t empty = {0, {}};
		return empty;
	}
	if (load_size < 0) load_size = 0;
	if (load_size >= NUM_LOAD_SIZES) load_size = NUM_LOAD_SIZES - 1;
	return program_actions[program].loads[load_size];
}
#endif
