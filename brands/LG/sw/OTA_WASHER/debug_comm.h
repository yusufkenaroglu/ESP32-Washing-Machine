/*
 * debug_comm.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#ifdef SIMULATOR_MODE_ENABLED
void sendSpriteAsJSON(TFT_eSprite &sprite, int width, int height) {
  uint16_t* buffer = (uint16_t*)sprite.getPointer();
  Serial.print("\"sprite\":\""); // Begin a single hex string
  for (int i = 0; i < width * height; i++) {
    char hex[5]; // 4 digits + null
    sprintf(hex, "%04X", buffer[i]);  // uppercase hex
    Serial.print(hex);
  }
  Serial.print("\"");
}

void printMachineStateJSON() {
  // Compose a JSON string with key machine state variables
  Serial.print("{");
  Serial.print("\"program_name\":\"");
  Serial.print(programmes[program_selector]);
  Serial.print("\",");
  Serial.print("\"motor_dir_value\":");
  Serial.print(motor_dir_value ? "\"CCW\"" : "\"CW\"");
  Serial.print(",");
  Serial.print("\"target_rpm\":");
  Serial.print(target_rpm);
  Serial.print(",");
  Serial.print("\"current_rpm\":");
  Serial.print(rpm);
  Serial.print(",");
  Serial.print("\"turns\":");
  Serial.print(turns);
  Serial.print(",");
  Serial.print("\"cycle_name\":\"");
  Serial.print(cycles[turns]);
  Serial.print("\",");
  Serial.print("\"program_stopped\":");
  Serial.print(program_stopped ? "true" : "false");
  Serial.print(",");
  Serial.print("\"powered_on\":");
  Serial.print(powered_on ? "true" : "false");
  Serial.print(",");
  Serial.print("\"door_is_open\":");
  Serial.print(door_is_open ? "true" : "false");
  Serial.print(",");
  Serial.print("\"drum_light\":");
  Serial.print(drum_light ? "true" : "false");
  Serial.print(",");
  Serial.print("\"ETA\":");
  Serial.print(ETA);
  Serial.print(",");
  Serial.print("\"seconds_elapsed\":");
  Serial.print(seconds_elapsed);
  Serial.print(",");
  Serial.print("\"fill_allowed\":");
  Serial.print(fill_allowed ? "true" : "false");
  Serial.print(",");
  Serial.print("\"drain_allowed\":");
  Serial.print(drain_allowed ? "true" : "false");
  Serial.print(",");
  Serial.print("\"muted\":");
  Serial.print(muted ? "true" : "false");
  Serial.print(",");
  sendSpriteAsJSON(img, IWIDTH, IHEIGHT);
  Serial.println("}");
}
#endif