/*
 * Sound.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
float attackTime = 0.02;  // Attack time in seconds
float decayTime = 0.041;    // Decay time in seconds
float sustainLevel = 0.75;  // Sustain level (0 to 1)
float releaseTime = 0.24;   // Release time in seconds
void generateSineWaveWithADSR(float frequency, int duration, float startAmplitude, float endAmplitude, int sampleRate) {

  int numSamples = sampleRate * duration / 1000;         // Number of samples for the given duration
  float amplitudeRange = startAmplitude - endAmplitude;  // Amplitude change over the duration

  int attackSamples = sampleRate * attackTime;
  int decaySamples = sampleRate * decayTime;
  int releaseSamples = sampleRate * releaseTime;
  int sustainSamples = numSamples - (attackSamples + decaySamples + releaseSamples);

  for (int i = 0; i < numSamples; ++i) {
    // Record the start time
    unsigned long startMicros = micros();

    // Calculate the time for this sample
    float t = (float)i / sampleRate;

    // Generate a sine wave value (normalized between 0 and 1)
    float sineValue = 0.5 * (1 + sin(2 * PI * frequency * t));

    // Calculate the current amplitude based on ADSR envelope
    float currentAmplitude;
    if (i < attackSamples) {
      // Attack phase
      currentAmplitude = (float)i / attackSamples;
    } else if (i < attackSamples + decaySamples) {
      // Decay phase
      float decayProgress = (float)(i - attackSamples) / decaySamples;
      currentAmplitude = startAmplitude * pow(sustainLevel, decayProgress);
    } else if (i < attackSamples + decaySamples + sustainSamples) {
      // Sustain phase
      currentAmplitude = sustainLevel;
    } else {
      // Release phase
      currentAmplitude = sustainLevel - ((float)(i - attackSamples - decaySamples - sustainSamples) / releaseSamples) * sustainLevel;
    }

    // Apply the amplitude to the sine wave value
    float outputValue = sineValue * currentAmplitude;

    // Convert the normalized value to an 8-bit DAC value (0-255)
    int dacValue = (int)(outputValue * 255);

    // Output the value to the DAC
    dacWrite(25, dacValue);

    // Calculate the elapsed time and adjust the delay accordingly
    unsigned long elapsedMicros = micros() - startMicros;
    int delayTime = (1000000 / sampleRate) - elapsedMicros;
    if (delayTime > 0) {
      delayMicroseconds(delayTime);
    }
  }
}

float on_sound_frequencies[] = { 1499, 1895.94, 2253.07, 1895.94, 2253, 2997.99 };
int on_sound_durations[] = { 233, 267, 132, 136, 200, 350 };
const float on_sound_startAmplitudes[] = { 1, 1, 1, 1, 1, 1 };
const float on_sound_endAmplitudes[] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.0 };

float off_sound_frequencies[] = { 2974.56, 2253, 1895.94, 2253, 1895.94, 1499 };
int off_sound_durations[] = { 233, 267, 132, 136, 200, 350 };
const float off_sound_startAmplitudes[] = { 1, 1, 1, 1, 1, 1 };
const float off_sound_endAmplitudes[] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.0 };

float select_sound_frequencies[] = { 2703.10 };
int select_sound_durations[] = { 250 };
const float select_sound_startAmplitudes[] = { 1.0 };
const float select_sound_endAmplitudes[] = { 0.0 };

float error_sound_frequencies[] = { 6031.15, 2253.07 };
int error_sound_durations[] = { 90, 240 };
const float error_sound_startAmplitudes[] = { 1.0, 1.0 };
const float error_sound_endAmplitudes[] = { 0.50, 0.0 };

float start_sound_frequencies[] = { 2253.07, 2533.27, 3383.53 };
int start_sound_durations[] = { 90, 103, 300 };
const float start_sound_startAmplitudes[] = { 1.0, 1.0, 1.0 };
const float start_sound_endAmplitudes[] = { 0.75, 0.70, 0.0 };

float stop_sound_frequencies[] = { 3383.53, 3009.71, 2253.07 };
int stop_sound_durations[] = { 96, 104, 300 };
const float stop_sound_startAmplitudes[] = { 1.0, 1.0, 1.0 };
const float stop_sound_endAmplitudes[] = { 0.75, 0.70, 0.0 };

float end_sound_frequencies[] = { 2253.07, 3009.71, 2830.57, 2521.55, 2253.07, 1895.94, 1999.81, 2253.07, 2521.55, 1685.90, 1895.94, 1999.81, 1895.94, 2253.07, 2253.07, 2997.99, 2830.57, 2521.55, 2253.07, 2997.99, 2997.99, 3371.81, 2997.99, 2830.57, 2521.55, 2830.57, 2997.99 };
int end_sound_durations[] = { 600, 200, 200, 200, 600, 600, 200, 200, 200, 200, 200, 200, 600, 600, 600, 200, 200, 200, 600, 600, 200, 200, 200, 200, 200, 200, 700 };
const float end_sound_startAmplitudes[] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
const float end_sound_endAmplitudes[] = { 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0 };
const int sampleRateMultiplier = 8;

void play_on_sound(void* params) {
  
  for (int i = 0; i < 6; ++i) {
    generateSineWaveWithADSR(on_sound_frequencies[i], on_sound_durations[i], on_sound_startAmplitudes[i], on_sound_endAmplitudes[i], on_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_select_sound(void* params) {
  for (int i = 0; i < 1; ++i) {
    generateSineWaveWithADSR(select_sound_frequencies[i], select_sound_durations[i], select_sound_startAmplitudes[i], select_sound_endAmplitudes[i], select_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_start_sound(void* params) {
  for (int i = 0; i < 3; ++i) {
    generateSineWaveWithADSR(start_sound_frequencies[i], start_sound_durations[i], start_sound_startAmplitudes[i], start_sound_endAmplitudes[i], start_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_error_sound(void* params) {
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 2; ++i) {
      generateSineWaveWithADSR(error_sound_frequencies[i], error_sound_durations[i], error_sound_startAmplitudes[i], error_sound_endAmplitudes[i], error_sound_frequencies[i] * sampleRateMultiplier);
    }
    delay(400);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_stop_sound(void* params) {
  for (int i = 0; i < 3; ++i) {
    generateSineWaveWithADSR(stop_sound_frequencies[i], stop_sound_durations[i], stop_sound_startAmplitudes[i], stop_sound_endAmplitudes[i], stop_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_off_sound(void* params) {
  for (int i = 0; i < 6; ++i) {
    generateSineWaveWithADSR(off_sound_frequencies[i], off_sound_durations[i], off_sound_startAmplitudes[i], off_sound_endAmplitudes[i], off_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}

void play_end_sound(void* params) {
  for (int i = 0; i < 28; ++i) {
    generateSineWaveWithADSR(end_sound_frequencies[i], end_sound_durations[i], end_sound_startAmplitudes[i], end_sound_endAmplitudes[i], end_sound_frequencies[i] * sampleRateMultiplier);
  }
  for(;;){
    vTaskDelete(NULL);
  }
}