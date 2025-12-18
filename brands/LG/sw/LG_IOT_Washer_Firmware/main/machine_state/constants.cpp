/*
 * constants.cpp
 * Program timing presets, statistical parameters, and program names
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#include "constants.h"

/*===========================================================================
 * Program Timing Arrays
 *===========================================================================*/
const ProgramProfile program_profiles[NUM_PROGRAMS] = {
    {
        .name = "Allergiene",
        .tumble_min = 110,
        .stop_min = 11,
        .default_temp_idx = 0,
        .min_temp_idx = 0,
        .max_temp_idx = 0,
        .default_spin_idx = 4,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 0,
        .min_soil_idx = 0,
        .max_soil_idx = 0,
    },
    {
        .name = "Sanitary",
        .tumble_min = 96,
        .stop_min = 10,
        .default_temp_idx = 5,
        .min_temp_idx = 5,
        .max_temp_idx = 5,
        .default_spin_idx = 4,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Bright Whites",
        .tumble_min = 66,
        .stop_min = 7,
        .default_temp_idx = 4,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 4,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Bulky/Large",
        .tumble_min = 57,
        .stop_min = 6,
        .default_temp_idx = 2,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 3,
        .min_spin_idx = 1,
        .max_spin_idx = 3,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Heavy Duty",
        .tumble_min = 89,
        .stop_min = 9,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 5,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 3,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Cotton/Normal",
        .tumble_min = 63,
        .stop_min = 6,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 5,
        .default_spin_idx = 4,
        .min_spin_idx = 2,
        .max_spin_idx = 5,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Jumbo Wash",
        .tumble_min = 57,
        .stop_min = 6,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 4,
        .min_spin_idx = 1,
        .max_spin_idx = 4,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Towels",
        .tumble_min = 57,
        .stop_min = 6,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 5,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Perm. Press",
        .tumble_min = 43,
        .stop_min = 4,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 3,
        .min_spin_idx = 2,
        .max_spin_idx = 4,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Hand Wash/Wool",
        .tumble_min = 50,
        .stop_min = 5,
        .default_temp_idx = 3,
        .min_temp_idx = 1,
        .max_temp_idx = 3,
        .default_spin_idx = 2,
        .min_spin_idx = 1,
        .max_spin_idx = 2,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 2,
    },
    {
        .name = "Delicates",
        .tumble_min = 42,
        .stop_min = 4,
        .default_temp_idx = 2,
        .min_temp_idx = 1,
        .max_temp_idx = 3,
        .default_spin_idx = 3,
        .min_spin_idx = 1,
        .max_spin_idx = 3,
        .default_soil_idx = 2,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {//CONTINUE FROM HERE
        .name = "Speed Wash",
        .tumble_min = 16,
        .stop_min = 2,
        .default_temp_idx = 4,
        .min_temp_idx = 1,
        .max_temp_idx = 4,
        .default_spin_idx = 5,
        .min_spin_idx = 1,
        .max_spin_idx = 5,
        .default_soil_idx = 1,
        .min_soil_idx = 1,
        .max_soil_idx = 3,
    },
    {
        .name = "Small Load",
        .tumble_min = 45,
        .stop_min = 4,
        .default_temp_idx = 3,
        .min_temp_idx = 3,
        .max_temp_idx = 3,
        .default_spin_idx = 4,
        .min_spin_idx = 4,
        .max_spin_idx = 4,
        .default_soil_idx = 2,
        .min_soil_idx = 2,
        .max_soil_idx = 2,
    },
    {
        .name = "Tub Clean",
        .tumble_min = 89,
        .stop_min = 9,
        .default_temp_idx = 0,
        .min_temp_idx = 0,
        .max_temp_idx = 0,
        .default_spin_idx = 0,
        .min_spin_idx = 0,
        .max_spin_idx = 0,
        .default_soil_idx = 0,
        .min_soil_idx = 0,
        .max_soil_idx = 0,
    },
};

const ProgramActionProfile program_actions[NUM_PROGRAMS] = {
    // Allergiene
    {{{3, {WASH_ACTION_TUMBLING, WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_STEPPING}}}},
    // Sanitary
    {{{3, {WASH_ACTION_TUMBLING, WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_STEPPING}}}},
    // Bright Whites
    {{{3, {WASH_ACTION_TUMBLING, WASH_ACTION_SWINGING, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_STEPPING}}}},
    // Bulky/Large
    {{{2, {WASH_ACTION_ROLLING, WASH_ACTION_TUMBLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_STEPPING}}}},
    // Heavy Duty
    {{{3, {WASH_ACTION_TUMBLING, WASH_ACTION_SCRUBBING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}}},
    // Cotton/Normal
    {{{2, {WASH_ACTION_TUMBLING, WASH_ACTION_SWINGING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Jumbo Wash
    {{{2, {WASH_ACTION_ROLLING, WASH_ACTION_TUMBLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_STEPPING}}}},
    // Towels
    {{{2, {WASH_ACTION_TUMBLING, WASH_ACTION_SCRUBBING}}, {3, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}, {3, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}}},
    // Perm. Press
    {{{2, {WASH_ACTION_TUMBLING, WASH_ACTION_SWINGING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Hand Wash/Wool
    {{{2, {WASH_ACTION_SWINGING, WASH_ACTION_STEPPING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_SWINGING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Delicates
    {{{2, {WASH_ACTION_SWINGING, WASH_ACTION_STEPPING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_SWINGING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Speed Wash
    {{{2, {WASH_ACTION_TUMBLING, WASH_ACTION_SCRUBBING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Small Load
    {{{2, {WASH_ACTION_TUMBLING, WASH_ACTION_SCRUBBING}}, {2, {WASH_ACTION_TUMBLING, WASH_ACTION_ROLLING}}, {2, {WASH_ACTION_ROLLING, WASH_ACTION_FILTRATION}}}},
    // Tub Clean
    {{{2, {WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}, {2, {WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}, {2, {WASH_ACTION_FILTRATION, WASH_ACTION_SCRUBBING}}}},
};

const char *const temperatures[6] = {"-", "TAP COLD", "COLD", "WARM", "HOT", "EXTRA HOT"};
const char *const spin_speeds[6] = {"-", "NO SPIN", "LOW", "MEDIUM", "HIGH", "EXTRA HIGH"};
const char *const soil_levels[4] = {"-", "LIGHT", "NORMAL", "HEAVY"};
const signed char additional_options[3] = {-1, 0, 1}; //-1 for N/A, 0 for disabled, 1 for enabled

/*===========================================================================
 * Program and Cycle Names
 *===========================================================================*/
const char *const cycle_names[NUM_CYCLES] = {
    "Detecting",
    "Washing",
    "Washing",
    "Washing",
    "Rinsing",
    "Rinsing",
    "Rinsing",
    "Final spinning"};
