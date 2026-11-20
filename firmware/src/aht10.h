#pragma once
#include <Arduino.h>

// Minimal AHT10 / AHT20 driver (I2C address 0x38).
namespace AHT10 {
  bool begin(int sda, int scl);
  bool read(float& tempC, float& humidity);
}
