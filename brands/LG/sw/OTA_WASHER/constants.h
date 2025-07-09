/*
 * constants.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#define circulation_pump 15 //OUTPUT -> screw terminal (+ and -)
#define drain_pump 27 //OUTPUT -> screw terminal (+ and -)
#define power_led 26 //OUTPUT -> B6B connector
#define drum_led 13 //OUTPUT -> P2 connector
#define start_stop_led 14 //OUTPUT -> B6B connector
#define fill_pump 12 //OUTPUT -> screw terminal (+ and -)
#define speaker 25 //OUTPUT -> screw terminal (+ and -)
#define power_button 33 //INPUT -> B6B connector
#define start_stop_button 32 //INPUT -> B6B connector
#define door_sensor 34 //INPUT -> screw terminal (+ and -)
#define odrive_tx 17
#define odrive_rx 16

const float mean_small = 486.36;     // Mean of first distribution
const float std_dev_small = 109.59;  // Standard deviation of first distribution
const float mean_medium = 503.87;    // Mean of first distribution
const float std_dev_medium = 84.25;  // Standard deviation of first distribution
const float mean_large = 594.33;     // Mean of first distribution
const float std_dev_large = 136.25;  // Standard deviation of first distribution
const float means[] = { mean_small, mean_medium, mean_large };
const float standard_deviations[] = { std_dev_small, std_dev_medium, std_dev_large };
const int inputs[] = { door_sensor };
const int outputs[] = { start_stop_led, power_led, drain_pump, circulation_pump, fill_pump };
const int tumble_durations[] = { 15, 10, 25, 10, 7, 15, 15, 15, 27, 25, 5, 2, 10, 8 };
const int stop_durations[] = { 7, 7, 7, 10, 15, 10, 15, 15, 3, 5, 10, 15, 5, 7 };
const int temperatures[] = { 0, 20, 30, 40, 50, 60, 70, 80, 90 };
const int default_temperatures[] = { 2, 2, 5, 3, -1, -1, -1, 3, 3, 3, 2, 2, 4, 3 };
const int max_temperatures[] = { 3, 3, 5, 3, 5, 5, 5, 5, 5, 3, 3, 2, 5, 4 };
const int spin_speeds[] = { 400, 600, 800, 1000, 1200, 1400, 1600 };
const int max_spin_speeds[] = { 4, 2, 6, 6, 6, 6, 6, 6, 4, 6, 2, 2, 6, 6 };
const char p1[] PROGMEM = "Cotton";
const char p2[] PROGMEM = "Heavy Duty";
const char p3[] PROGMEM = "Bulky";
const char p4[] PROGMEM = "Whites";
const char p5[] PROGMEM = "Sanitary";
const char p6[] PROGMEM = "Allergiene";
const char p7[] PROGMEM = "Tub Clean";
const char p8[] PROGMEM = "Jumbo Wash";
const char p9[] PROGMEM = "Towels";
const char p10[] PROGMEM = "Perm. Press";
const char p11[] PROGMEM = "Wool";
const char p12[] PROGMEM = "Delicates";
const char p13[] PROGMEM = "Speed Wash";
const char p14[] PROGMEM = "Small Load";
const char *const programmes[] PROGMEM = { p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14 };
const char c0[] PROGMEM = "Detecting";
const char c1[] PROGMEM = "Washing"; //Phase 1
const char c2[] PROGMEM = "Washing"; //Phase 2
const char c3[] PROGMEM = "Washing"; //Phase 3
const char c4[] PROGMEM = "Rinsing";
const char c5[] PROGMEM = "Rinsing";
const char c6[] PROGMEM = "Rinsing";
const char c7[] PROGMEM = "Final spinning";
const char *const cycles[] PROGMEM = { c0, c1, c2, c3, c4, c5, c6, c7 };

int duration0 = 600;            // Load Detection
int duration1 = 90;             // Washing Phase 1
int duration2 = 5;              // Washing Phase 2
int duration3 = 600;            // Washing Phase 3
int duration4 = duration3 / 2;  // Rinse 1
int duration5 = duration3 / 2;  // Rinse 2
int duration6 = duration3 / 2;  // Rinse 3
int duration7 = 300;            // Final spinning
int durations[] = { duration0, duration1, duration2, duration3, duration4, duration5, duration6, duration7 };

int instance_durations[] = { 0, 0, 0, 0, 0, 0, 0, 0 };


