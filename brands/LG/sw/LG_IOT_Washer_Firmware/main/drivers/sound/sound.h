/*
 * sound.h
 * DAC-based sound generation with ADSR envelope
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Sound Configuration
 *===========================================================================*/

#define SOUND_SAMPLE_RATE       8000    // 8 kHz sample rate
#define SOUND_DAC_CHANNEL       1       // DAC channel (GPIO25)

/*===========================================================================
 * ADSR Envelope Parameters
 *===========================================================================*/

typedef struct {
    uint16_t attack_ms;     // Time to reach peak (ms)
    uint16_t decay_ms;      // Time to reach sustain level (ms)
    uint8_t  sustain_level; // Sustain level (0-255)
    uint16_t release_ms;    // Time to fade out (ms)
} adsr_envelope_t;

// Predefined envelopes
extern const adsr_envelope_t ADSR_BEEP;      // Short beep
extern const adsr_envelope_t ADSR_ALERT;     // Alert tone
extern const adsr_envelope_t ADSR_SOFT;      // Soft tone
extern const adsr_envelope_t ADSR_SHARP;     // Sharp attack

/*===========================================================================
 * Sound API
 *===========================================================================*/

/**
 * @brief Initialize sound subsystem
 * @return ESP_OK on success
 */
esp_err_t sound_init(void);

/**
 * @brief Play a tone with specified frequency and duration
 * @param frequency Frequency in Hz (20-20000)
 * @param duration_ms Duration in milliseconds
 * @param envelope ADSR envelope to use (nullptr for default)
 */
void sound_play_tone(uint16_t frequency, uint16_t duration_ms, const adsr_envelope_t *envelope);

/**
 * @brief Play a predefined sound effect
 * @param effect_id Effect ID (see SOUND_EFFECT_* constants)
 */
void sound_play_effect(uint8_t effect_id);

/**
 * @brief Stop any currently playing sound
 */
void sound_stop(void);

/**
 * @brief Check if sound is currently playing
 * @return true if playing
 */
bool sound_is_playing(void);

/**
 * @brief Set master volume
 * @param volume Volume level (0-255)
 */
void sound_set_volume(uint8_t volume);

/*===========================================================================
 * Predefined Sound Effects
 *===========================================================================*/

#define SOUND_EFFECT_STARTUP        0
#define SOUND_EFFECT_BUTTON_PRESS   1
#define SOUND_EFFECT_CYCLE_START    2
#define SOUND_EFFECT_CYCLE_END      3
#define SOUND_EFFECT_ERROR          4
#define SOUND_EFFECT_DOOR_OPEN      5
#define SOUND_EFFECT_WATER_FILL     6
#define SOUND_EFFECT_ON             7
#define SOUND_EFFECT_OFF            8
#define SOUND_EFFECT_SELECT         9
#define SOUND_EFFECT_STOP           10

#ifdef __cplusplus
}
#endif
