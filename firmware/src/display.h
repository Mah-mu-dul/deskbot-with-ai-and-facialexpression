#pragma once
#include <Arduino.h>

// Animated robot face + status bar on the SPI TFT.
// Rendering happens in its own FreeRTOS task into a PSRAM canvas.
namespace Display {
  bool begin();
  void startTask();                          // after begin()
  void bootMessage(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);  // before task starts
}
