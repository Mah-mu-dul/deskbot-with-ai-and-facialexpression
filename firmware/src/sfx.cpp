#include "sfx.h"
#include "audio.h"
#include "buzzer.h"

namespace {
using C = Audio::Chirp;

// {start Hz, end Hz, ms, vibrato}. {0,0,ms,0} = pause.
const C HAPPY[]     = {{900, 1400, 90, 0}, {0, 0, 40, 0}, {1150, 1800, 130, 0}};
const C LOVE[]      = {{1000, 1300, 160, 40}, {0, 0, 30, 0}, {1100, 1450, 160, 40}, {0, 0, 30, 0}, {1300, 900, 260, 30}};
const C SAD[]       = {{760, 560, 260, 12}, {0, 0, 60, 0}, {600, 380, 420, 18}};
const C ANGRY[]     = {{260, 200, 180, 30}, {0, 0, 50, 0}, {280, 190, 260, 40}};
const C SURPRISED[] = {{480, 1900, 150, 0}, {0, 0, 30, 0}, {1900, 1700, 70, 0}};
const C SLEEPY[]    = {{700, 300, 650, 6}, {0, 0, 120, 0}, {340, 280, 300, 0}};
const C CONFUSED[]  = {{800, 1150, 140, 0}, {1150, 700, 160, 0}, {0, 0, 40, 0}, {900, 1000, 120, 25}};
const C FOCUSED[]   = {{1200, 1200, 60, 0}, {0, 0, 50, 0}, {1600, 1600, 70, 0}};
const C WARNING[]   = {{1500, 1500, 120, 0}, {0, 0, 70, 0}, {1500, 1500, 120, 0}, {0, 0, 70, 0}, {1200, 900, 220, 0}};

bool s_audioOk = false;
Face s_last = Face::Neutral;
uint32_t s_lastPlay = 0;

#define PLAY(arr) Audio::playChirps(arr, sizeof(arr) / sizeof(arr[0]))
}  // namespace

void Sfx::play(Face f) {
  if (!s_audioOk) {            // buzzer fallback
    switch (f) {
      case Face::Happy: case Face::Love: Buzzer::play("success"); break;
      case Face::Sad: case Face::Sleepy: Buzzer::play("break"); break;
      case Face::Angry: case Face::Warning: Buzzer::play("warning"); break;
      case Face::Surprised: case Face::Confused: Buzzer::play("double"); break;
      default: Buzzer::play("beep"); break;
    }
    return;
  }
  switch (f) {
    case Face::Happy:     PLAY(HAPPY); break;
    case Face::Love:      PLAY(LOVE); break;
    case Face::Sad:       PLAY(SAD); break;
    case Face::Angry:     PLAY(ANGRY); break;
    case Face::Surprised: PLAY(SURPRISED); break;
    case Face::Sleepy:    PLAY(SLEEPY); break;
    case Face::Confused:  PLAY(CONFUSED); break;
    case Face::Focused:   PLAY(FOCUSED); break;
    case Face::Warning:   PLAY(WARNING); break;
    default: break;        // neutral: quiet
  }
}

void Sfx::update(bool audioOk) {
  s_audioOk = audioOk;
  Face e = expressionFace();
  if (e == s_last) return;
  s_last = e;
  if (!g_state.sfx || g_state.activity != Activity::Idle) return;
  if (millis() - s_lastPlay < 2500) return;      // don't chatter when moods flicker
  s_lastPlay = millis();
  play(e);
}
