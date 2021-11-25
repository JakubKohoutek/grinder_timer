#include "voltage.h"

float readVoltage () {
  long samples       = 0;
  int  samplesLength = 100;

  for (int i = 0; i < samplesLength; i++) {
    samples += analogRead(A0);
    delay(2);
  }

  int sensorValue = samples / samplesLength;

  // Calculation of voltage is based on the values of voltage divider resistors
  int vccResistor = 100;
  int gndResistor = 22;
  float maxVoltage = 0.836;
  float resistorsRatio = (float)gndResistor / (vccResistor + gndResistor);
  float voltage = (maxVoltage * (float)sensorValue / 1024.0) / resistorsRatio;

  #ifdef DEBUG
    Serial.println(String("Sensor value: ") + String(sensorValue) + String(", voltage: ") + String(voltage));
  #endif

  return voltage;
}