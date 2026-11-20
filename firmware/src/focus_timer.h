#pragma once
#include <Arduino.h>
#include "state.h"

enum class FocusEvent : uint8_t { None, WorkDone, BreakDone };

// Pomodoro-style focus timer. Runs entirely on the device; state is
// mirrored into g_state and reported to the server with every frame.
namespace FocusTimer {
  void start(uint16_t workMin, uint16_t breakMin);
  void stop();
  void pause();
  void resume();
  void togglePause();
  FocusEvent tick();          // call from loop()
  bool active();              // work, break or paused
  uint16_t workMinutes();
  uint16_t breakMinutes();
}
