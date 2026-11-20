#pragma once
#include <Arduino.h>

// Non-blocking melody player for a 2-pin buzzer (passive recommended;
// an active buzzer also works but every note sounds the same pitch).
namespace Buzzer {
  void begin();
  void update();                    // call often from loop()
  void play(const char* pattern);   // "beep","double","warning","success","start","error","listen","break"
  void toneBlocking(uint16_t freq, uint16_t ms);
  bool busy();
}
