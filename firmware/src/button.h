#pragma once
#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, Click, DoubleClick, LongPress };

// Debounced push button with click / double-click / long-press detection.
//   Click        -> talk to the bot (voice assistant)
//   Long press   -> start / stop focus timer
//   Double click -> pause / resume focus, or toggle the sensor screen
namespace Button {
  void begin();
  ButtonEvent poll();     // call every loop
  bool isDown();          // raw state
  void clear();           // forget pending clicks (after a blocking action)
}
