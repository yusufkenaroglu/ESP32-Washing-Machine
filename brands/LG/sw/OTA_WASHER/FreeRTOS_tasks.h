/*
 * FreeRTOS_tasks.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
void task0(void *parameters) {
  vTaskSuspend(decrement_handle);
  vTaskResume(task_drum_motor_handle);
  //analogWrite(motor_brk, 0);
  /*while (1) {
    for (int i = 0; i < 2; i++) {
      xTaskCreate(tumble_wash1, "tumble_wash1", 2048, NULL, 1, &tumble_handle);
      int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
      vTaskDelay(delay_dur / portTICK_PERIOD_MS);
      while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
      vTaskDelete(tumble_handle);
      target_rpm = 0;
      analogWrite(circulation_pump, 0);
      vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
    }

    while (!detect_unbalanced_load()) {
      vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
    for (int i = 200; i < 800; i++) {
      target_rpm = i;
      vTaskDelay(25 / portTICK_PERIOD_MS);
    }
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    for (int i = 800; i >= 0; i--) {
      target_rpm = i;
      vTaskDelay(5 / portTICK_PERIOD_MS);
    }
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);*/
  //drain();
  //while (drain_allowed) {
  //  vTaskDelay(100 / portTICK_PERIOD_MS);
  //}
  int samples_per_second = 50;
  int delay_duration_ms = 1000 / samples_per_second;
  int tumble_duration_ms = 2000;
  int num_trials = 6;
  //analogWrite(motor_dir, 4095 * motor_dir_value);
  int size = num_trials * samples_per_second * (tumble_duration_ms / 1000);
  //analogWrite(motor_brk, 0);
  float *samples = (float *)malloc(size * sizeof(float));
  int index = 0;  // Index for storing samples
  //analogWrite(motor_dir, 4095 * motor_dir_value);
  for (int i = 0; i < num_trials; i++) {
    target_rpm = 60;
    for (int j = 0; j < samples_per_second * (tumble_duration_ms / 1000); j++) {
      // Store the RPM sample
      if (index < size) {
        samples[index] = pwm_value;
        index++;
      }

      // Wait for the next sample
      vTaskDelay(delay_duration_ms / portTICK_PERIOD_MS);
    }

    // Stop the motor
    target_rpm = 0;

    // Wait for 3 seconds before the next measurement cycle
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
  // Calculate mean and standard deviation
  float mean = calculateMean(samples, size);
  float sd = calculateStd(samples, size, mean);

  Serial.println("Mean:");
  Serial.println(mean);
  Serial.println("Standard Deviation:");
  Serial.println(sd);
  free(samples);
  for (int i = 0; i < 3; i++) {
    // Calculate the area between the two normal distributions
    float lowerLimit = min(means[i] - 10 * standard_deviations[i], mean_new - 10 * std_dev_new);  // Adjust as needed
    float upperLimit = max(means[i] + 10 * standard_deviations[i], mean_new + 10 * std_dev_new);  // Adjust as needed

    float areaBetween = 0.0;
    float stepSize = 0.05;  // Width of intervals for calculating area

    for (float x = lowerLimit; x < upperLimit; x += stepSize) {
      float pdf1 = normalPDF(x, means[i], standard_deviations[i]);
      float pdf2 = normalPDF(x, mean_new, std_dev_new);
      areaBetween += min(pdf1, pdf2) * stepSize;  // Use the minimum value at each point for the overlap area
    }
    areas[i] = areaBetween;
    Serial.println(areaBetween);
  }
  ETA_available = true;
  target_rpm = 0;
  instance_durations[0] = 0;
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void task2(void *parameters) {  // display detected load size for 3 seconds
  instance_durations[2] = 0;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void task1(void *parameters) {  // fill the drum with water
  start_fill();
  vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
  stop_fill();
  instance_durations[1] = 0;
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


//WashParams was here

void generic_wash_action(void *params) {
  // params is expected to be a pointer to a struct with wash parameters
  struct WashParams {
    int tumble_rpm;
    int tumble_duration_ms;
    int stop_duration_ms;
    float pump_on_start_frac; // 0.0 to 1.0 (fraction of tumble duration to start pump)
    float pump_on_end_frac;   // 0.0 to 1.0 (fraction of tumble duration to stop pump)
    bool alternate_direction; // true if direction should alternate each cycle
    int pump_pwm;             // PWM value for the circulation pump
    int pump_on_steps;        // For step/scrub/swing: number of pump on/off cycles
    int pump_on_step_ms;      // For step/scrub/swing: ms per on/off cycle
  };
  WashParams *wp = (WashParams*)params;

  for (;;) {
    if (wp->alternate_direction) {
      vTaskSuspend(task_drum_motor_handle);
      vTaskDelay(150 / portTICK_PERIOD_MS);
      motor_dir_value ^= 1;
      vTaskResume(task_drum_motor_handle);
    }

    target_rpm = wp->tumble_rpm;

    // For step/scrub/swing: repeated on/off cycles
    if (wp->pump_on_steps > 0) {
      for (int i = 0; i < wp->pump_on_steps; i++) {
        analogWrite(circulation_pump, wp->pump_pwm);
        vTaskDelay(wp->pump_on_step_ms / portTICK_PERIOD_MS);
        analogWrite(circulation_pump, 0);
        vTaskDelay(wp->pump_on_step_ms / portTICK_PERIOD_MS);
        if (wp->alternate_direction) {
          motor_dir_value ^= 1;
        }
      }
      vTaskResume(task_drum_motor_handle);
      target_rpm = 0;
      vTaskDelay(wp->stop_duration_ms / portTICK_PERIOD_MS);
      continue;
    }

    // For tumble/filtration/rolling: pump on for a fraction of tumble duration
    int tumble_ms = wp->tumble_duration_ms;
    int pump_on_start = tumble_ms * wp->pump_on_start_frac;
    int pump_on_end = tumble_ms * wp->pump_on_end_frac;

    // Pre-pump phase
    if (pump_on_start > 0)
      vTaskDelay(pump_on_start / portTICK_PERIOD_MS);

    // Pump ON phase
    if (pump_on_end > pump_on_start) {
      analogWrite(circulation_pump, wp->pump_pwm);
      vTaskDelay((pump_on_end - pump_on_start) / portTICK_PERIOD_MS);
      analogWrite(circulation_pump, 0);
    }

    // Post-pump phase
    if (tumble_ms > pump_on_end)
      vTaskDelay((tumble_ms - pump_on_end) / portTICK_PERIOD_MS);

    target_rpm = 0;
    vTaskDelay(wp->stop_duration_ms / portTICK_PERIOD_MS);

    // For rolling, resume drum motor after each cycle
    if (wp->alternate_direction)
      vTaskResume(task_drum_motor_handle);
  }
}

void task3(void *parameters) {  // Load adjust (for 20 seconds)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  for (;;) {
    if (instance_durations[3] >= 60) {
      int random_value1 = random(1, 19);
      int random_value2 = random(2, 4);
      switch (random_value1) {
        case 1:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &tumble1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 2:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &tumble2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 3:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &tumble3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;

        case 4:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &filtration1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);

            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 5:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &filtration2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 6:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &filtration3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 7:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &scrub1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 8:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &scrub2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 9:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &scrub3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 10:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &step1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 11:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &step2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 12:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &step3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 13:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &swing_wash1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 14:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &swing_wash2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 15:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &swing_wash3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 16:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &rolling_wash1_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 17:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &rolling_wash2_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;
        case 18:
          for (int i = 0; i < random_value2; i++) {
            xTaskCreate(generic_wash_action, "wash", 2048, &rolling_wash3_params, 1, &tumble_handle);
            int delay_dur = (tumble_durations[program_selector] + stop_durations[program_selector]) * 2500;
            vTaskDelay(delay_dur / portTICK_PERIOD_MS);
            while (rpm) vTaskDelay(100 / portTICK_PERIOD_MS);
            vTaskDelete(tumble_handle);
            target_rpm = 0;
            analogWrite(circulation_pump, 0);
            vTaskDelay(stop_durations[program_selector] * 1000 / portTICK_PERIOD_MS);
          }
          break;

        default:
          break;
      }
    } else {
      vTaskDelete(tumble_handle);
      vTaskSuspend(decrement_handle);
      analogWrite(circulation_pump, 0);
      analogWrite(fill_pump, 0);
      target_rpm = 0;
      vTaskDelay(500 / portTICK_PERIOD_MS);
      drain();
      while (drain_allowed) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      while (!detect_unbalanced_load()) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      for (int i = 200; i < 250; i++) {
        target_rpm = i;
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      drain_allowed = true;
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      drain_allowed = false;
      for (int i = 251; i < 1000; i++) {
        target_rpm = i;
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }

      vTaskResume(decrement_handle);
      instance_durations[turns] = 0;
    }
  }
}

void task4(void *parameters) {  // Washing (for durations[program_selector] seconds)

  target_rpm = 0;

  for (;;) {
    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }

    while (target_rpm < 60) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 4095; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    target_rpm = 0;

    while (target_rpm < 100) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 45;
    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 4095; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);
  }
}

void task5(void *parameters) {  // Washing (for durations[program_selector] seconds)

  target_rpm = 0;

  for (;;) {
    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }

    while (target_rpm < 60) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 4095; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    target_rpm = 0;

    while (target_rpm < 100) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 45;
    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 4095; i >= 2410; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);
  }
}

void task6(void *parameters) {  // Washing (for durations[program_selector] seconds)

  target_rpm = 0;

  for (;;) {
    for (int i = 2410; i < 4096; i++) {
      analogWrite(circulation_pump, i);
      delay(1);
    }

    while (target_rpm < 60) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 255; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    target_rpm = 0;

    while (target_rpm < 100) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 45;
    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 255; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);
  }
}

void task7(void *parameters) {  // Washing (for durations[program_selector] seconds)

  target_rpm = 0;

  for (;;) {
    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }

    while (target_rpm < 60) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 255; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    for (int i = 150; i < 256; i++) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    target_rpm = 0;

    while (target_rpm < 100) {
      target_rpm++;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 45;
    vTaskDelay((tumble_durations[program_selector] * 1000) / portTICK_PERIOD_MS);

    while (target_rpm && rpm) {
      target_rpm--;
      vTaskDelay(30 / portTICK_PERIOD_MS);
    }

    target_rpm = 0;
    pwm = 0;
    for (int i = 255; i >= 150; i--) {
      analogWrite(circulation_pump, i);
      delay(5);
    }
    analogWrite(circulation_pump, 0);
    vTaskDelay((stop_durations[program_selector] * 1000) / portTICK_PERIOD_MS);
  }
}

void decrement(void *parameters) {
  for (;;) {
    if (powered_on) {
      if (program_stopped) {
        start_stop_lit = !start_stop_lit;
        digitalWrite(start_stop_led, start_stop_lit);
      } else {
        start_stop_lit = false;
        ETA = 0;
        for (int i = 0; i < 8; i++) {
          ETA += instance_durations[i];
        }
        if (instance_durations[turns] > 0 && ETA) {
          instance_durations[turns] -= 1;
          seconds_elapsed += 1;
        } else {
          turns += 1;
          update_active_task();  // Switch to next task when turns changes
        }
      }
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (!program_stopped) vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void task_pumps(void *parameters) {
  for (;;) {
#ifdef BALANCE_DETECTION_ENABLED
    if (drain_allowed) {
      for (int i = 0; i < 50; i++) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        accY[i] = a.acceleration.y;
        vTaskDelay(5 / portTICK_PERIOD_MS);  // Adjust delay as necessary to control sampling rate
      }
      float accY_mean = calculateMean(accY, 50);
      float accY_std = calculateStd(accY, 50, accY_mean);
      Serial.println(accY_std);
      if (accY_std > 0.05) {
        stop_drain();
      }
      analogWrite(drain_pump, 1400);
    } else {
      analogWrite(drain_pump, 0);
    }
    if (fill_allowed) {
      analogWrite(fill_pump, 4095);
    } else {
      analogWrite(fill_pump, 0);
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
#else
    // No MPU6050: run drain pump for a fixed time, then stop
    const int drain_time_ms = 15000;  // 15 seconds, adjust as needed
    if (drain_allowed) {
      start_drain();
      vTaskDelay(drain_time_ms / portTICK_PERIOD_MS);
      stop_drain();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Main loop delay
#endif
  }
}

void task_drum_motor(void *params) {
#ifndef SIMULATOR_MODE_ENABLED
  bool bandwidth_high = false;  // false = bandwidth = 10, true = bandwidth = 100
#endif
  for (;;) {
#ifndef SIMULATOR_MODE_ENABLED
    volatile float rps = motor_dir_value ? target_rpm / 60.0 : -target_rpm / 60.0;
    odrive.setVelocity(rps);

    float abs_rps = fabs(rps);

    if (abs_rps > 5.0 && !bandwidth_high) {
      //odrive.sendCommand("w axis0.motor.config.current_control_bandwidth 100");
      bandwidth_high = true;
    } else if (abs_rps <= 5.0 && bandwidth_high) {
      //odrive.sendCommand("w axis0.motor.config.current_control_bandwidth 10");
      bandwidth_high = false;
    }
#endif
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}