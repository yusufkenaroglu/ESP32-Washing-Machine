/*
 * sound.c
 * DAC-based sound generation with ADSR envelope
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Why this implementation uses a task + sequence model:
 * - Sound generation is implemented with a dedicated task and lightweight
 *   sequence spawner to keep audio timing isolated from control logic. This
 *   allows audio sequences to be scheduled without blocking the caller and
 *   keeps real-time waveform generation in one place.
 * - Using predefined ADSR envelopes and a sine lookup table simplifies
 *   waveform generation and ensures deterministic CPU usage on the DAC
 *   oneshot driver.
 */

#include "sound.h"
#include "drivers/gpio_hal/gpio_hal.h"

#include <math.h>
#include <string.h>
#include <cstdlib>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/dac_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "sound";

/*===========================================================================
 * Predefined ADSR Envelopes
 *===========================================================================*/

const adsr_envelope_t ADSR_BEEP = {
    .attack_ms = 5,
    .decay_ms = 20,
    .sustain_level = 200,
    .release_ms = 30
};

const adsr_envelope_t ADSR_ALERT = {
    .attack_ms = 10,
    .decay_ms = 50,
    .sustain_level = 180,
    .release_ms = 100
};

const adsr_envelope_t ADSR_SOFT = {
    .attack_ms = 50,
    .decay_ms = 100,
    .sustain_level = 150,
    .release_ms = 200
};

const adsr_envelope_t ADSR_SHARP = {
    .attack_ms = 2,
    .decay_ms = 10,
    .sustain_level = 220,
    .release_ms = 20
};

/*===========================================================================
 * Sine Wave Lookup Table (256 entries, 8-bit)
 *===========================================================================*/

static const uint8_t sine_table[256] = {
    128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
    176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
    218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
    245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
    255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
    245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
    218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
    176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
    128,124,121,118,115,112,109,106,103,100, 97, 93, 90, 88, 85, 82,
     79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
     37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11,
     10,  9,  7,  6,  5,  5,  4,  3,  2,  2,  1,  1,  1,  0,  0,  0,
      0,  0,  0,  0,  1,  1,  1,  2,  2,  3,  4,  5,  5,  6,  7,  9,
     10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35,
     37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76,
     79, 82, 85, 88, 90, 93, 97,100,103,106,109,112,115,118,121,124
};

/*===========================================================================
 * State Variables
 *===========================================================================*/

static volatile bool s_playing = false;
static volatile bool s_stop_requested = false;
static uint8_t s_volume = 255;  // Master volume (0-255)
static SemaphoreHandle_t s_sound_mutex = NULL;
static TaskHandle_t s_sound_task_handle = NULL;
static dac_oneshot_handle_t s_dac_handle = nullptr;

// Current tone parameters
static uint16_t s_frequency = 0;
static uint16_t s_duration_ms = 0;
static adsr_envelope_t s_envelope;

/*===========================================================================
 * Sound Sequences (Ported from Arduino)
 *===========================================================================*/

static const float on_sound_frequencies[] = { 1499, 1895.94, 2253.07, 1895.94, 2253, 2997.99 };
static const int on_sound_durations[] = { 233, 267, 132, 136, 200, 350 };
static const float on_sound_startAmplitudes[] = { 1, 1, 1, 1, 1, 1 };
static const float on_sound_endAmplitudes[] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.0 };

static const float off_sound_frequencies[] = { 2974.56, 2253, 1895.94, 2253, 1895.94, 1499 };
static const int off_sound_durations[] = { 233, 267, 132, 136, 200, 350 };
static const float off_sound_startAmplitudes[] = { 1, 1, 1, 1, 1, 1 };
static const float off_sound_endAmplitudes[] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.0 };

static const float select_sound_frequencies[] = { 2703.10 };
static const int select_sound_durations[] = { 250 };
static const float select_sound_startAmplitudes[] = { 1.0 };
static const float select_sound_endAmplitudes[] = { 0.0 };

static const float error_sound_frequencies[] = { 6031.15, 2253.07 };
static const int error_sound_durations[] = { 90, 240 };
static const float error_sound_startAmplitudes[] = { 1.0, 1.0 };
static const float error_sound_endAmplitudes[] = { 0.50, 0.0 };

static const float start_sound_frequencies[] = { 2253.07, 2533.27, 3383.53 };
static const int start_sound_durations[] = { 90, 103, 300 };
static const float start_sound_startAmplitudes[] = { 1.0, 1.0, 1.0 };
static const float start_sound_endAmplitudes[] = { 0.75, 0.70, 0.0 };

static const float stop_sound_frequencies[] = { 3383.53, 3009.71, 2253.07 };
static const int stop_sound_durations[] = { 96, 104, 300 };
static const float stop_sound_startAmplitudes[] = { 1.0, 1.0, 1.0 };
static const float stop_sound_endAmplitudes[] = { 0.75, 0.70, 0.0 };

static const float end_sound_frequencies[] = { 2253.07, 3009.71, 2830.57, 2521.55, 2253.07, 1895.94, 1999.81, 2253.07, 2521.55, 1685.90, 1895.94, 1999.81, 1895.94, 2253.07, 2253.07, 2997.99, 2830.57, 2521.55, 2253.07, 2997.99, 2997.99, 3371.81, 2997.99, 2830.57, 2521.55, 2830.57, 2997.99 };
static const int end_sound_durations[] = { 600, 200, 200, 200, 600, 600, 200, 200, 200, 200, 200, 200, 600, 600, 600, 200, 200, 200, 600, 600, 200, 200, 200, 200, 200, 200, 700 };
static const float end_sound_startAmplitudes[] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
static const float end_sound_endAmplitudes[] = { 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0 };

/*===========================================================================
 * Internal Functions
 *===========================================================================*/

typedef struct {
    const float *freqs;
    const int *durs;
    const float *start_amps;
    const float *end_amps;
    int count;
    int repeat;
} sound_sequence_t;

static void play_sequence_task(void *arg)
{
    auto *seq = static_cast<sound_sequence_t *>(arg);

    for (int r = 0; r < seq->repeat; r++) {
        for (int i = 0; i < seq->count; i++) {
            adsr_envelope_t env = ADSR_BEEP;
            env.attack_ms = seq->durs[i] * 0.1; // Approximate attack
            env.decay_ms = seq->durs[i] * 0.2;  // Approximate decay
            env.sustain_level = (uint8_t)(seq->end_amps[i] * 255);
            env.release_ms = seq->durs[i] * 0.7; // Approximate release
            
            sound_play_tone((uint16_t)seq->freqs[i], (uint16_t)seq->durs[i], &env);
            
            // Wait for tone to finish
            vTaskDelay(pdMS_TO_TICKS(seq->durs[i] + 10));
        }
        if (seq->repeat > 1) vTaskDelay(pdMS_TO_TICKS(400));
    }
    
    free(seq);
    vTaskDelete(NULL);
}

static void spawn_sequence(const float *freqs, const int *durs, const float *start_amps, const float *end_amps, int count, int repeat)
{
    sound_sequence_t *seq = static_cast<sound_sequence_t *>(malloc(sizeof(sound_sequence_t)));
    if (seq) {
        seq->freqs = freqs;
        seq->durs = durs;
        seq->start_amps = start_amps;
        seq->end_amps = end_amps;
        seq->count = count;
        seq->repeat = repeat;
        
        xTaskCreate(play_sequence_task, "snd_seq", 2048, seq, 5, NULL);
    }
}

/**
 * @brief Calculate ADSR envelope value at given time
 */
static uint8_t calculate_envelope(uint32_t elapsed_ms, uint16_t total_duration_ms, 
                                   const adsr_envelope_t *env)
{
    uint16_t attack = env->attack_ms;
    uint16_t decay = env->decay_ms;
    uint8_t sustain = env->sustain_level;
    uint16_t release = env->release_ms;
    
    // Attack phase
    if (elapsed_ms < attack) {
        return (255 * elapsed_ms) / attack;
    }
    
    // Decay phase
    if (elapsed_ms < attack + decay) {
        uint32_t decay_progress = elapsed_ms - attack;
        return 255 - ((255 - sustain) * decay_progress / decay);
    }
    
    // Sustain phase
    uint16_t sustain_end = total_duration_ms - release;
    if (elapsed_ms < sustain_end) {
        return sustain;
    }
    
    // Release phase
    if (elapsed_ms < total_duration_ms) {
        uint32_t release_progress = elapsed_ms - sustain_end;
        return sustain - (sustain * release_progress / release);
    }
    
    return 0;
}

/**
 * @brief Sound generation task
 */
static void sound_task(void *pvParameters)
{
    const uint32_t sample_period_us = 1000000 / SOUND_SAMPLE_RATE;
    
    while (1) {
        // Wait for play request
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (s_frequency == 0 || s_duration_ms == 0) {
            s_playing = false;
            continue;
        }
        
        s_playing = true;
        s_stop_requested = false;
        
        ESP_LOGD(TAG, "Playing tone: %d Hz, %d ms", s_frequency, s_duration_ms);
        
        int64_t start_time = esp_timer_get_time();
        uint32_t duration_us = s_duration_ms * 1000;
        
        // Phase accumulator for wave generation
        uint32_t phase = 0;
        uint32_t phase_increment = (s_frequency * 256) / SOUND_SAMPLE_RATE;
        
        while (!s_stop_requested) {
            int64_t current_time = esp_timer_get_time();
            uint32_t elapsed_us = current_time - start_time;
            
            if (elapsed_us >= duration_us) {
                break;
            }
            
            // Calculate envelope
            uint32_t elapsed_ms = elapsed_us / 1000;
            uint8_t envelope = calculate_envelope(elapsed_ms, s_duration_ms, &s_envelope);
            
            // Get sine wave sample
            uint8_t wave = sine_table[phase & 0xFF];
            
            // Apply envelope and volume
            // wave is 0-255 centered at 128
            // Convert to signed, apply envelope and volume, convert back
            int16_t sample = (int16_t)wave - 128;
            sample = (sample * envelope * s_volume) / (255 * 255);
            sample += 128;
            
            // Clamp and output
            if (sample < 0) sample = 0;
            if (sample > 255) sample = 255;
            
            dac_oneshot_output_voltage(s_dac_handle, (uint8_t)sample);
            
            // Advance phase
            phase += phase_increment;
            
            // Wait for next sample
            // Use busy-wait for accurate timing
            while ((esp_timer_get_time() - current_time) < sample_period_us) {
                // Tight loop
            }
        }
        
        // Output silence (midpoint)
        dac_oneshot_output_voltage(s_dac_handle, 128);
        
        s_playing = false;
        ESP_LOGD(TAG, "Tone finished");
    }
}

/*===========================================================================
 * Public API
 *===========================================================================*/

esp_err_t sound_init(void)
{
    // Create mutex
    s_sound_mutex = xSemaphoreCreateMutex();
    if (s_sound_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    s_dac_handle = gpio_hal_get_dac_handle();
    if (!s_dac_handle) {
        ESP_LOGE(TAG, "DAC handle is null. Call app_dac_init() before sound_init()");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Output silence
    dac_oneshot_output_voltage(s_dac_handle, 128);
    
    // Create sound task (high priority for timing accuracy)
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        sound_task,
        "sound_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // High priority
        &s_sound_task_handle,
        0  // Pin to core 0
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sound task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Sound subsystem initialized");
    return ESP_OK;
}

void sound_play_tone(uint16_t frequency, uint16_t duration_ms, const adsr_envelope_t *envelope)
{
    if (xSemaphoreTake(s_sound_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire sound mutex");
        return;
    }
    
    // Stop current sound if playing
    if (s_playing) {
        s_stop_requested = true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Set parameters
    s_frequency = frequency;
    s_duration_ms = duration_ms;
    
    if (envelope != NULL) {
        memcpy(&s_envelope, envelope, sizeof(adsr_envelope_t));
    } else {
        memcpy(&s_envelope, &ADSR_BEEP, sizeof(adsr_envelope_t));
    }
    
    xSemaphoreGive(s_sound_mutex);
    
    // Notify sound task
    if (s_sound_task_handle != NULL) {
        xTaskNotifyGive(s_sound_task_handle);
    }
}

void sound_play_effect(uint8_t effect_id)
{
    switch (effect_id) {
        case SOUND_EFFECT_STARTUP:
            // Ascending arpeggio
            sound_play_tone(523, 100, &ADSR_SHARP);  // C5
            vTaskDelay(pdMS_TO_TICKS(120));
            sound_play_tone(659, 100, &ADSR_SHARP);  // E5
            vTaskDelay(pdMS_TO_TICKS(120));
            sound_play_tone(784, 200, &ADSR_SOFT);   // G5
            break;
            
        case SOUND_EFFECT_BUTTON_PRESS:
            sound_play_tone(1000, 50, &ADSR_SHARP);
            break;
            
        case SOUND_EFFECT_CYCLE_START:
            spawn_sequence(start_sound_frequencies, start_sound_durations, start_sound_startAmplitudes, start_sound_endAmplitudes, 3, 1);
            break;
            
        case SOUND_EFFECT_CYCLE_END:
            spawn_sequence(end_sound_frequencies, end_sound_durations, end_sound_startAmplitudes, end_sound_endAmplitudes, 27, 1);
            break;
            
        case SOUND_EFFECT_ERROR:
            spawn_sequence(error_sound_frequencies, error_sound_durations, error_sound_startAmplitudes, error_sound_endAmplitudes, 2, 3);
            break;
            
        case SOUND_EFFECT_DOOR_OPEN:
            sound_play_tone(600, 100, &ADSR_SHARP);
            break;
            
        case SOUND_EFFECT_WATER_FILL:
            // Bubbling sound (random frequencies)
            for (int i = 0; i < 5; i++) {
                uint16_t freq = 300 + (esp_random() % 200);
                sound_play_tone(freq, 80, &ADSR_SHARP);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;

        case SOUND_EFFECT_ON:
            spawn_sequence(on_sound_frequencies, on_sound_durations, on_sound_startAmplitudes, on_sound_endAmplitudes, 6, 1);
            break;

        case SOUND_EFFECT_OFF:
            spawn_sequence(off_sound_frequencies, off_sound_durations, off_sound_startAmplitudes, off_sound_endAmplitudes, 6, 1);
            break;

        case SOUND_EFFECT_SELECT:
            spawn_sequence(select_sound_frequencies, select_sound_durations, select_sound_startAmplitudes, select_sound_endAmplitudes, 1, 1);
            break;

        case SOUND_EFFECT_STOP:
            spawn_sequence(stop_sound_frequencies, stop_sound_durations, stop_sound_startAmplitudes, stop_sound_endAmplitudes, 3, 1);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown sound effect: %d", effect_id);
            break;
    }
}

void sound_stop(void)
{
    s_stop_requested = true;
}

bool sound_is_playing(void)
{
    return s_playing;
}

void sound_set_volume(uint8_t volume)
{
    s_volume = volume;
}
