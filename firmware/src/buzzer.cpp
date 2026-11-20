#include "buzzer.h"
#include "config.h"

namespace {
struct Note { uint16_t freq; uint16_t ms; };   // freq 0 = rest

const Note P_BEEP[]    = {{2200, 80}};
const Note P_DOUBLE[]  = {{2200, 70}, {0, 60}, {2200, 70}};
const Note P_WARNING[] = {{2800, 150}, {0, 80}, {2800, 150}, {0, 80}, {2800, 300}};
const Note P_SUCCESS[] = {{1319, 100}, {1568, 100}, {2093, 220}};
const Note P_START[]   = {{1047, 90}, {1319, 90}, {1568, 90}, {2093, 160}};
const Note P_ERROR[]   = {{400, 200}, {0, 50}, {300, 300}};
const Note P_LISTEN[]  = {{1760, 60}};
const Note P_BREAK[]   = {{2093, 120}, {1568, 120}, {1319, 120}, {1047, 240}};

const Note* s_seq = nullptr;
size_t s_len = 0, s_idx = 0;
uint32_t s_noteEnd = 0;

void out(uint16_t f) {
  if (f) ledcWriteTone(BUZZER_LEDC_CH, f);
  else   ledcWriteTone(BUZZER_LEDC_CH, 0);
}

void start(const Note* seq, size_t len) {
  s_seq = seq; s_len = len; s_idx = 0;
  out(seq[0].freq);
  s_noteEnd = millis() + seq[0].ms;
}
}  // namespace

#define START(arr) start(arr, sizeof(arr) / sizeof(arr[0]))

void Buzzer::begin() {
  ledcSetup(BUZZER_LEDC_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
  out(0);
}

void Buzzer::update() {
  if (!s_seq) return;
  if ((int32_t)(millis() - s_noteEnd) < 0) return;
  s_idx++;
  if (s_idx >= s_len) { out(0); s_seq = nullptr; return; }
  out(s_seq[s_idx].freq);
  s_noteEnd = millis() + s_seq[s_idx].ms;
}

void Buzzer::play(const char* p) {
  if (!p) return;
  if      (!strcmp(p, "double"))  START(P_DOUBLE);
  else if (!strcmp(p, "warning")) START(P_WARNING);
  else if (!strcmp(p, "success")) START(P_SUCCESS);
  else if (!strcmp(p, "start"))   START(P_START);
  else if (!strcmp(p, "error"))   START(P_ERROR);
  else if (!strcmp(p, "listen"))  START(P_LISTEN);
  else if (!strcmp(p, "break"))   START(P_BREAK);
  else                            START(P_BEEP);
}

void Buzzer::toneBlocking(uint16_t freq, uint16_t ms) {
  s_seq = nullptr;
  out(freq);
  delay(ms);
  out(0);
}

bool Buzzer::busy() { return s_seq != nullptr; }
