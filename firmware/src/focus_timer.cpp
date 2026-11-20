#include "focus_timer.h"
#include "config.h"

namespace {
uint16_t s_work = FOCUS_WORK_MIN, s_break = FOCUS_BREAK_MIN;
FocusPhase s_phase = FocusPhase::Off, s_pausedFrom = FocusPhase::Off;
uint32_t s_endMs = 0, s_pausedLeftMs = 0, s_totalMs = 0;

void beginPhase(FocusPhase p) {
  s_phase = p;
  s_totalMs = (uint32_t)(p == FocusPhase::Work ? s_work : s_break) * 60000UL;
  s_endMs = millis() + s_totalMs;
}

void publish() {
  g_state.focusPhase = s_phase;
  g_state.focusTotalSec = s_totalMs / 1000;
  uint32_t left = 0;
  if (s_phase == FocusPhase::Paused) left = s_pausedLeftMs;
  else if (s_phase != FocusPhase::Off) {
    int32_t d = (int32_t)(s_endMs - millis());
    left = d > 0 ? d : 0;
  }
  g_state.focusRemainingSec = (left + 999) / 1000;
}
}  // namespace

void FocusTimer::start(uint16_t workMin, uint16_t breakMin) {
  if (workMin)  s_work  = constrain(workMin, 1, 180);
  if (breakMin) s_break = constrain(breakMin, 1, 60);
  beginPhase(FocusPhase::Work);
  publish();
}

void FocusTimer::stop() {
  s_phase = FocusPhase::Off;
  s_totalMs = 0;
  publish();
}

void FocusTimer::pause() {
  if (s_phase != FocusPhase::Work && s_phase != FocusPhase::Break) return;
  int32_t d = (int32_t)(s_endMs - millis());
  s_pausedLeftMs = d > 0 ? d : 0;
  s_pausedFrom = s_phase;
  s_phase = FocusPhase::Paused;
  publish();
}

void FocusTimer::resume() {
  if (s_phase != FocusPhase::Paused) return;
  s_phase = s_pausedFrom;
  s_endMs = millis() + s_pausedLeftMs;
  publish();
}

void FocusTimer::togglePause() {
  if (s_phase == FocusPhase::Paused) resume(); else pause();
}

FocusEvent FocusTimer::tick() {
  FocusEvent ev = FocusEvent::None;
  if ((s_phase == FocusPhase::Work || s_phase == FocusPhase::Break) &&
      (int32_t)(s_endMs - millis()) <= 0) {
    if (s_phase == FocusPhase::Work) {
      g_state.focusSessions = g_state.focusSessions + 1;
      beginPhase(FocusPhase::Break);
      ev = FocusEvent::WorkDone;
    } else {
      beginPhase(FocusPhase::Work);
      ev = FocusEvent::BreakDone;
    }
  }
  publish();
  return ev;
}

bool FocusTimer::active() { return s_phase != FocusPhase::Off; }
uint16_t FocusTimer::workMinutes() { return s_work; }
uint16_t FocusTimer::breakMinutes() { return s_break; }
