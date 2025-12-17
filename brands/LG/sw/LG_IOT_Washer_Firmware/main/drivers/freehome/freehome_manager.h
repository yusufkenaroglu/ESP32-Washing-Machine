/*
 * freehome_manager.h
 * FreeHome integration manager (skeleton)
 *
 * Responsibilities:
 * - Persist FreeHome pairing/enable state in NVS
 * - Provide simple APIs for UI and other modules
 * - Run a background task when enabled+linked (stub)
 *
 * Copyright 2025
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FREEHOME_STATUS_DISABLED = 0,
    FREEHOME_STATUS_ENABLED = 1,
    FREEHOME_STATUS_PROVISIONING = 2,
    FREEHOME_STATUS_CONNECTING = 3,
    FREEHOME_STATUS_LINKED = 4,
    FREEHOME_STATUS_ERROR = 5
} freehome_status_t;

// Initialize FreeHome manager (must be called after NVS init)
int freehome_init(void);

// Start interactive setup wizard (non-blocking)
int freehome_start_setup(void);

// Query state
bool freehome_is_linked(void);
bool freehome_is_enabled(void);
freehome_status_t freehome_get_status(void);
const char *freehome_get_device_id(void);

// Enable/disable feature (persisted)
int freehome_set_enabled(bool enabled);

// Unlink device and erase persisted FreeHome data
int freehome_unlink(void);

// Register a callback to be notified when internal state changes
typedef void (*freehome_state_cb_t)(freehome_status_t new_state);
bool freehome_register_state_callback(freehome_state_cb_t cb);

#ifdef __cplusplus
}
#endif
