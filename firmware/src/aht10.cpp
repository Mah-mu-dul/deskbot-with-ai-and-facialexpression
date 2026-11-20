#include "aht10.h"
#include <Wire.h>

namespace {
const uint8_t ADDR = 0x38;

bool cmd(uint8_t a, uint8_t b, uint8_t c) {
  Wire.beginTransmission(ADDR);
  Wire.write(a); Wire.write(b); Wire.write(c);
  return Wire.endTransmission() == 0;
}
}  // namespace

bool AHT10::begin(int sda, int scl) {
  Wire.begin(sda, scl, 100000);
  delay(40);
  Wire.beginTransmission(ADDR);
  if (Wire.endTransmission() != 0) return false;
  // Soft reset, then calibrate. AHT10 uses 0xE1, AHT20 uses 0xBE - send both.
  Wire.beginTransmission(ADDR); Wire.write(0xBA); Wire.endTransmission();
  delay(25);
  if (!cmd(0xE1, 0x08, 0x00)) cmd(0xBE, 0x08, 0x00);
  delay(20);
  return true;
}

bool AHT10::read(float& tempC, float& humidity) {
  if (!cmd(0xAC, 0x33, 0x00)) return false;
  delay(80);
  uint8_t d[6];
  for (int tries = 0; tries < 5; tries++) {
    if (Wire.requestFrom(ADDR, (uint8_t)6) != 6) return false;
    for (int i = 0; i < 6; i++) d[i] = Wire.read();
    if (!(d[0] & 0x80)) break;   // not busy
    delay(20);
  }
  if (d[0] & 0x80) return false;
  uint32_t rawH = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t rawT = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
  humidity = rawH * 100.0f / 1048576.0f;
  tempC = rawT * 200.0f / 1048576.0f - 50.0f;
  return humidity >= 0 && humidity <= 100;
}
