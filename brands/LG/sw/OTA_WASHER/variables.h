/*
 * variables.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
float areas[3] = { 0, 0, 0 };
float accY[50] = {};

float mean_new = 12.0;    // Mean of second distribution
float std_dev_new = 6.0;  // Standard deviation of second distribution
volatile unsigned long lastTime, diff, currentTime = 0;
float revolutions = 8.0 / 360.0;

int pwm = 0;

bool muted = false;
bool ETA_available = false;
bool sound_playing = false;
bool drain_allowed = false;
bool fill_allowed = false;
bool motor_controllable = true;
bool door_is_open = false;
bool drum_light = false;
int num_samples;
int num_fills = 0;

bool motor_dir_value = false;  //clockwise when false, counter-clockwise when true
volatile float rpm = 0.0;
volatile float previousRPM = 0.0;
volatile int pwm_value = 0;
volatile int target_rpm = 0;
volatile float rpmChange = 0.0;

int logo_allowed;
int seconds_elapsed = 0;
bool start_stop_lit = false;

char program_selector = 0;  // ranges from 0 to 11. Value corresponds with the indices of program arrays.
int turns = 0;              // ranges from 0 to 9. turns => task(turns + 1).
int program_stopped = 1;    // 0 program not running. 1 when program running.
int powered_on = 0;         // 0 when the machine is off 1 when the machine is on.
int wash_time = 5;          // ranges from 10 to 30. The machine will perform (Tumble->Stop->Tumble->Stop) "washTime" times.

int temperature_selector = 0;
int spin_speed_selector = 4;


typedef struct {
  int tumble_rpm;
  int tumble_duration_ms;
  int stop_duration_ms;
  float pump_on_start_frac; // 0.0 to 1.0
  float pump_on_end_frac;   // 0.0 to 1.0
  bool alternate_direction;
  int pump_pwm;
  int pump_on_steps;
  int pump_on_step_ms;
} WashParams;

WashParams tumble1_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams tumble2_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams tumble3_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};
WashParams filtration1_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams filtration2_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams filtration3_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};
WashParams scrub1_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams scrub2_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams scrub3_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};
WashParams step1_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams step2_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams step3_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};
WashParams swing_wash1_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams swing_wash2_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams swing_wash3_params = {60, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};
WashParams rolling_wash1_params = {35, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.0, 1.0, true, 4095, 0, 0};
WashParams rolling_wash2_params = {36, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.25, 0.75, true, 4095, 0, 0};
WashParams rolling_wash3_params = {39, tumble_durations[program_selector] * 1000, stop_durations[program_selector] * 1000, 0.75, 1.0, true, 4095, 0, 0};