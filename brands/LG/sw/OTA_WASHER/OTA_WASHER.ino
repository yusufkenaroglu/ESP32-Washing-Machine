/*
 * OTA_WASHER.ino
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
//----------FLAGS----------
#define ESP32_RTOS
#define USE_INTRANET
//#define SIMULATOR_MODE_ENABLED
#define WIFI_ENABLED
#define BALANCE_DETECTION_ENABLED
//----------END FLAGS----------

#ifndef SIMULATOR_MODE_ENABLED
#include <ODriveUART.h>
#include <ODriveCAN.h>
#include <SoftwareSerial.h>
#endif

#include "Sound.h"
#include <qrcode.h>
#ifdef WIFI_ENABLED
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include "OTA.h"
#include <WebServer.h>
#include "HTML.h"
#endif

#include <math.h>
//#include <Wire.h>
#ifdef BALANCE_DETECTION_ENABLED
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#endif
#include "stats.h"
#include "constants.h"
#include "variables.h"
#include "graphic_assets.h"

TaskHandle_t task0_handle, task1_handle, task2_handle, task3_handle, task4_handle, task5_handle, task6_handle, task7_handle = NULL;
TaskHandle_t task_drum_motor_handle = NULL;
TaskHandle_t task_pumps_handle = NULL;
TaskHandle_t decrement_handle = NULL;
TaskHandle_t display_handle = NULL;
TaskHandle_t sound_handle = NULL;
TaskHandle_t tumble_handle = NULL;

#include "fonts.h"

#include <TFT_eSPI.h>  

#define IWIDTH 188
#define IHEIGHT 107


#ifndef SIMULATOR_MODE_ENABLED
SoftwareSerial odrive_serial(odrive_tx, odrive_rx);
ODriveUART odrive(odrive_serial);
#endif
unsigned long baudrate = 4800;  // Must match what you configure on the ODrive (see docs for details)

TFT_eSPI tft = TFT_eSPI();  // Invoke library
TFT_eSprite img = TFT_eSprite(&tft);
int ETA = 0;
#include "debug_comm.h"


// Variables
int i = 0;
#ifdef BALANCE_DETECTION_ENABLED
Adafruit_MPU6050 mpu;
#endif

/*----------------------------WIFI----------------------------*/

#ifdef WIFI_ENABLED
WiFiManager wifiManager;
WebServer server(80);
bool client_connected = false;
#endif


#ifdef WIFI_ENABLED
void configModeCallback(WiFiManager *myWiFiManager) {
  if (!client_connected) {
    //Serial.println("Entered config mode");
    //Serial.println(WiFi.softAPIP());
    // Generate QR code for the device's initial IP
  }
}


void startWebServer() {
  server.on("/", SendWebsite);
  server.on("/xml", SendXML);
  server.on("/UPDATE_SLIDER", UpdateSlider);
  server.on("/BUTTON_0", ProcessButton_0);
  server.on("/BUTTON_1", ProcessButton_1);
  server.begin();
}
#endif


void check_buttons() {

  volatile short temp1 = 0;
  for (int i = 0; i < 10; i++) {
    temp1 += digitalRead(door_sensor);
    delay(5 / portTICK_PERIOD_MS);
  }
  door_is_open = bool(temp1);

  if (digitalRead(power_button)) {
    while (digitalRead(power_button)) {
      delay(50);
    }
#ifdef WIFI_ENABLED
    ProcessButton_0();
#endif
  }
  if (digitalRead(start_stop_button)) {
    while (digitalRead(start_stop_button)) {
      delay(50);
    }
#ifdef WIFI_ENABLED
    ProcessButton_1();
#endif
  }
}

int calculate_sum() {
  int sum = 0;
  for (int i = 0; i < 11; i++) {
    if (i == 3) {
      sum += 2 * wash_time * (tumble_durations[program_selector] + stop_durations[program_selector]);
    } else {
      sum += durations[i];
    }
  }
  return sum;
}


#include "display.h"

//task_display was here


#include "FreeRTOS_tasks.h"


void update_active_task() {
  TaskHandle_t task_handles[] = {
    task0_handle, task1_handle, task2_handle, task3_handle,
    task4_handle, task5_handle, task6_handle, task7_handle
  };
  for (int i = 0; i < 8; i++) vTaskSuspend(task_handles[i]);
  if (!program_stopped && turns >= 0 && turns < 8) {
    vTaskResume(task_handles[turns]);
    vTaskResume(decrement_handle);
  } else {
    vTaskSuspend(decrement_handle);
  }
}


// variables to store measure data and sensor states
bool LED0 = false;
char XML[2048];
char buf[32];
/*----------WEB_UI-----------*/

void setup() {
#ifndef SIMULATOR_MODE_ENABLED
  odrive_serial.begin(baudrate);
  delay(100);
#endif
  analogWrite(circulation_pump, 0);
  Serial.begin(500000);
#ifdef BALANCE_DETECTION_ENABLED
  while (!mpu.begin()) {
    Serial.println("waiting on mpu");
    delay(100);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
#endif

  disableCore0WDT();
  disableCore1WDT();
#ifndef SIMULATOR_MODE_ENABLED
  while (odrive.getState() == AXIS_STATE_UNDEFINED) {
    odrive.setState(AXIS_STATE_CLOSED_LOOP_CONTROL);
    Serial.println(odrive.getParameterAsFloat("vbus_voltage"));
    delay(100);
  }
#endif


  analogWriteResolution(12);
  analogWriteFrequency(19500);

#ifdef WIFI_ENABLED
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect("LG_ThinQ")) {
    tft.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }
  if (!client_connected) {
    //Serial.println("Connected to network");
    //Serial.println(WiFi.localIP());
  }
  String ssid = WiFi.SSID();
  String password = WiFi.psk();
  setupOTA("LG OTA", ssid.c_str(), password.c_str());
  startWebServer();
#endif
  for (int i = 0; i < sizeof(inputs) / sizeof(int); i++) {
    pinMode(inputs[i], INPUT);
  }
  pinMode(power_button, INPUT);
  pinMode(start_stop_button, INPUT);
  for (int i = 0; i < sizeof(outputs) / sizeof(int); i++) {
    pinMode(outputs[i], OUTPUT);
  }
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  img.createSprite(IWIDTH, IHEIGHT);
  digitalWrite(start_stop_led, LOW);
  xTaskCreate(task_pumps, "task_pumps", 1024 * 4, NULL, 1, &task_pumps_handle);
  xTaskCreate(task_display, "display", 1024 * 3, NULL, 1, &display_handle);
  xTaskCreate(task0, "task0", 2048, NULL, 1, &task0_handle);
  vTaskSuspend(task0_handle);
  xTaskCreate(task1, "task1", 2048, NULL, 1, &task1_handle);
  vTaskSuspend(task1_handle);
  xTaskCreate(task2, "task2", 1024, NULL, 1, &task2_handle);
  vTaskSuspend(task2_handle);
  xTaskCreate(task3, "task3", 2048, NULL, 1, &task3_handle);
  vTaskSuspend(task3_handle);
  xTaskCreate(task4, "task4", 1024, NULL, 1, &task4_handle);
  vTaskSuspend(task4_handle);
  xTaskCreate(task5, "task5", 1024, NULL, 1, &task5_handle);
  vTaskSuspend(task5_handle);
  xTaskCreate(task6, "task6", 1024, NULL, 1, &task6_handle);
  vTaskSuspend(task6_handle);
  xTaskCreate(task7, "task7", 1024, NULL, 1, &task7_handle);
  vTaskSuspend(task7_handle);
  xTaskCreate(task_drum_motor, "task_drum_motor", 1024, NULL, 1, &task_drum_motor_handle);
  xTaskCreate(decrement, "decrement", 1024, NULL, 1, &decrement_handle);
}

void loop() {
  check_buttons();
  if (!program_stopped && !ETA) {
    for (int i = 0; i < 8; i++) {
      instance_durations[i] = durations[i];
      ETA += instance_durations[i];
    }
  }
#ifdef WIFI_ENABLED
  server.handleClient();
#endif
}

#ifdef WIFI_ENABLED
void UpdateSlider() {
  String t_state = server.arg("VALUE");
  char temp = t_state.toInt();
  if (temp != program_selector) xTaskCreate(play_select_sound, "play_select_sound", 2048, NULL, 100, &sound_handle);
  program_selector = temp;
  strcpy(buf, "");
  sprintf(buf, "%d", program_selector);
  sprintf(buf, buf);
  server.send(200, "text/plain", buf); 
}

void ProcessButton_0() {
  LED0 = !LED0;
  on_off();
  if (LED0) {
    server.send(200, "text/plain", "1");  //Send web page
  } else {
    server.send(200, "text/plain", "0");  //Send web page
  }
}

void ProcessButton_1() {
  if (powered_on) {
    program_stopped ^= 1;
    update_active_task();  // Ensure tasks are updated when program is stopped/started
#ifdef SIMULATOR_MODE_ENABLED
    printMachineStateJSON();
#endif
    if (program_stopped) {
      // If stopping, play stop sound
      if (!muted) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        xTaskCreate(play_stop_sound, "play_stop_sound", 2048, NULL, 100, &sound_handle);
      }
    } else {
      // If starting, ensure door is closed and play start sound
      while (door_is_open) {

        if (!muted) {
          vTaskDelay(50 / portTICK_PERIOD_MS);
          xTaskCreate(play_error_sound, "play_error_sound", 2048, NULL, 100, &sound_handle);
          check_buttons();
        }
        vTaskDelay(1600 / portTICK_PERIOD_MS);
      }
      if (!muted) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        xTaskCreate(play_start_sound, "play_start_sound", 2048, NULL, 100, &sound_handle);
      }

      digitalWrite(start_stop_led, HIGH);
    }
  }
  server.send(200, "text/plain", "");  //Send web page
}

void SendWebsite() {
  client_connected = true;
  server.send(200, "text/html", PAGE_MAIN);
}

void SendXML() {
  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  if (!powered_on)
    sprintf(buf, "<ETA>--:--</ETA>\n");
  else
    sprintf(buf, "<ETA>%d:%02d minutes</ETA>\n", !seconds_elapsed ? calculate_sum() / 60 : ETA / 60, !seconds_elapsed ? calculate_sum() % 60 : ETA % 60);

  strcat(XML, buf);

  sprintf(buf, "<WS>%s</WS>\n", !powered_on ? "OFF" : (!seconds_elapsed ? "Ready" : String(cycles[turns])));
  strcat(XML, buf);

  strcat(XML, "<LED>");
  strcat(XML, LED0 ? "1" : "0");
  strcat(XML, "</LED>\n");

  strcat(XML, "</Data>\n");
  server.send(200, "text/xml", XML);
}
#endif

// Handles the main power button logic, toggling the machine on/off and managing startup/shutdown effects
void on_off() {
  if (powered_on % 2 == 0) {
    // Powering on: reset state, light up indicators, play sound, and animate drum LED
    powered_on = 1;
    analogWrite(power_led, 4095);
    ETA = 0;
    seconds_elapsed = 0;
    turns = 0;
    logo_allowed = true;
    if (!muted) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      xTaskCreate(play_on_sound, "play_on_sound", 2048, NULL, 100, &sound_handle);
    }
    delay(1000);
    if (true) {
      for (int i = 0; i < 3072; i += 64) {
        analogWrite(drum_led, i);
        delay(2);
      }
      drum_light = true;
    }
    logo_allowed = false;
  } else {
    // Powering off: reset state, play shutdown sound, and fade out drum LED
    ETA = 0;
    turns = 0;
    seconds_elapsed = 0;
    program_stopped = 1;
    logo_allowed = true;
    if (!muted) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      xTaskCreate(play_off_sound, "play_off_sound", 2048, NULL, 100, &sound_handle);
    }
    delay(1000);
    if (drum_light) {
      for (int i = 3072; i >= 0; i -= 64) {
        analogWrite(drum_led, i);
        delay(2);
      }
      drum_light = false;
    }
    logo_allowed = false;
    digitalWrite(start_stop_led, LOW);
    analogWrite(power_led, 0);
    delay(100);
    powered_on = 0;
  }
}

#ifdef BALANCE_DETECTION_ENABLED
// Uses MPU6050 sensor data to detect if the load is unbalanced by measuring drum displacement while distributing garments
// Returns true if the load is balanced, false if unbalanced
bool detect_unbalanced_load() {

  for (int i = 40; i < 150; i++) {
    target_rpm = i;
    delay(75);
  }
  vTaskDelay(2000);
  float max_displacement = 0;
  for (int i = 0; i < 100; i++) {
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    float displacement = 0.5 * accel.acceleration.y * (1 / (target_rpm / 60.0)) * (1 / (target_rpm / 60.0));

    // Convert displacement from m/s^2 to cm
    displacement *= 100;

    // Track the maximum displacement encountered
    if (abs(displacement) > max_displacement) {
      max_displacement = abs(displacement);
    }
    Serial.print("Displacement: ");
    Serial.println(displacement);
  }

  Serial.print("Max Displacement: ");
  Serial.println(max_displacement);

  // If displacement exceeds threshold, consider load unbalanced and stop distribution
  if (max_displacement > 2.0) {
    target_rpm = 0;
    pwm_value = 0;
    return false;  
  }

  return true;  
}
#else
// Stub for when balance detection is disabled: always returns true
bool detect_unbalanced_load() {
  return true;
}
#endif

// Enables water fill by setting the control flag
void start_fill() {
  fill_allowed = true;
}

// Disables water fill by clearing the control flag
void stop_fill() {
  fill_allowed = false;
}

// Initiates the drain process (wrapper for start_drain)
void drain() {
  start_drain();
}

// Activates the drain pump with a randomized cleaning pulse sequence (only for auditory effects), then enables draining
void start_drain() {
  char rand_drain_clean_val = random(16, 20);
  for (char i = 0; i < rand_drain_clean_val; i++) {
    analogWrite(drain_pump, 4095);
    delay(30);
    analogWrite(drain_pump, 0);
    delay(20);
  }
  drain_allowed = true;
}

// Disables the drain pump and draining
void stop_drain() {
  drain_allowed = false;
}