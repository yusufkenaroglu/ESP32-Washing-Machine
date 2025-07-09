/*
 * display.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
void display_main_or_options_screen(int sum) {
  if (powered_on) {
    if (logo_allowed) {
      img.fillSmoothRoundRect(0, 0, 188, 107, 2, 0xb7ff);
      img.setSwapBytes(true);
      img.pushImage(0, 7, 186, 90, lg_logo);
      img.setSwapBytes(false);
    } else {
      img.fillSmoothRoundRect(0, 0, 188, 107, 2, 0xb7ff);
      if (program_stopped) {
        img.loadFont(LGSmart32);
        img.drawString(String(programmes[program_selector]), 10, 48);
        img.unloadFont();
      } else {
        img.loadFont(LGSmart32);
        img.drawString(String(cycles[turns]), 10, 48);
        img.unloadFont();
        img.setSwapBytes(true);
        img.pushImage(111, 1, 70, 21, door_lock);
        img.setSwapBytes(false);
      }
      img.drawFastHLine(9, 22, 172, TFT_BLACK);
      img.drawFastHLine(9, 77, 172, TFT_BLACK);

      img.drawFastVLine(51, 78, 21, TFT_BLACK);
      img.drawFastVLine(93, 78, 21, TFT_BLACK);
      img.drawFastVLine(138, 78, 21, TFT_BLACK);
      img.setSwapBytes(true);

      img.pushImage(9, 1, 23, 20, turbowash);
      img.pushImage(33, 1, 24, 20, drumlight);
      img.pushImage(98, 26, 40, 19, est_time_remaining);

      img.setSwapBytes(false);
      img.drawRoundRect(97, 25, 79, 22, 1, TFT_BLACK);
      img.setTextColor(TFT_BLACK);

      img.loadFont(LGSmart20);
      if (!ETA_available) img.drawString("--:--", 144, 27);
      else if (sum / 60 >= 10) {
        img.drawString("0:" + String(sum / 60), 141, 29);
      } else {
        img.drawString("0:0" + String(sum / 60), 141, 29);
      }
      img.unloadFont();
    }
  } else img.fillSmoothRoundRect(0, 0, 188, 107, 2, 0x4206);
  #ifndef SIMULATOR_MODE_ENABLED
  img.pushSprite(71, 44);
  #else
  printMachineStateJSON();
  #endif
}

void task_display(void *parameters) {
  for (;;) {
    int param = calculate_sum();
    if (!seconds_elapsed) display_main_or_options_screen(param);
    else display_main_or_options_screen(ETA);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}