/*
 * stats.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
float normalPDF(float x, float mean, float stdDev) {
  return 1.0 / (stdDev * sqrt(2 * PI)) * exp(-0.5 * pow((x - mean) / stdDev, 2));
}

float normalCDF(float x, float mean, float stdDev) {
  const int numSteps = 10000;
  float sum = 0.0;
  float dx = x / numSteps;

  for (int i = 0; i <= numSteps; ++i) {
    float xi = i * dx;
    float fxi = normalPDF(xi, mean, stdDev);

    if (i == 0 || i == numSteps) {
      sum += 0.5 * fxi;
    } else {
      sum += fxi;
    }
  }
  return sum * dx;
}

float calculateMean(float* data, int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += data[i];
  }
  return sum / size;
}

float calculateStd(float* data, int size, float mean) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += pow(data[i] - mean, 2);
  }
  return sqrt(sum / size);
}