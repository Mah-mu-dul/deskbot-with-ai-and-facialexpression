#include "state.h"

BotState g_state;

static const char* FACE_NAMES[] = {
  "neutral", "happy", "sad", "angry", "surprised", "sleepy", "love",
  "focused", "listening", "thinking", "speaking", "warning", "confused"
};

const char* faceName(Face f) {
  uint8_t i = (uint8_t)f;
  return i < (uint8_t)Face::COUNT ? FACE_NAMES[i] : "neutral";
}

Face faceFromName(const char* name, Face fallback) {
  if (!name) return fallback;
  for (uint8_t i = 0; i < (uint8_t)Face::COUNT; i++) {
    if (strcasecmp(name, FACE_NAMES[i]) == 0) return (Face)i;
  }
  return fallback;
}

const char* focusPhaseName(FocusPhase p) {
  switch (p) {
    case FocusPhase::Work:   return "work";
    case FocusPhase::Break:  return "break";
    case FocusPhase::Paused: return "paused";
    default:                 return "off";
  }
}

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static char     s_caption[160] = {0};
static uint32_t s_captionUntil = 0;
static char     s_events[128] = {0};

void setCaption(const String& text, uint32_t durationMs) {
  // The TFT's built-in font is ASCII only: show a placeholder for Bangla text.
  bool ascii = true;
  for (size_t i = 0; i < text.length(); i++) if ((uint8_t)text[i] >= 0x80) { ascii = false; break; }
  portENTER_CRITICAL(&s_mux);
  strlcpy(s_caption, ascii ? text.c_str() : "( Bangla )", sizeof(s_caption));
  s_captionUntil = millis() + durationMs;
  portEXIT_CRITICAL(&s_mux);
}

bool getCaption(char* out, size_t outLen) {
  bool active;
  portENTER_CRITICAL(&s_mux);
  active = s_caption[0] && (int32_t)(s_captionUntil - millis()) > 0;
  if (active) strlcpy(out, s_caption, outLen);
  portEXIT_CRITICAL(&s_mux);
  return active;
}

void pushEvent(const char* ev) {
  portENTER_CRITICAL(&s_mux);
  if (s_events[0]) strlcat(s_events, ",", sizeof(s_events));
  strlcat(s_events, ev, sizeof(s_events));
  portEXIT_CRITICAL(&s_mux);
}

String takeEvents() {
  char copy[sizeof(s_events)];
  portENTER_CRITICAL(&s_mux);
  strlcpy(copy, s_events, sizeof(copy));
  s_events[0] = 0;
  portEXIT_CRITICAL(&s_mux);
  return String(copy);
}

Face currentFace() {
  switch (g_state.activity) {
    case Activity::Listening: return Face::Listening;
    case Activity::Thinking:  return Face::Thinking;
    case Activity::Speaking:  return Face::Speaking;
    default: break;
  }
  return expressionFace();
}

Face expressionFace() {
  if ((int32_t)(g_state.overrideUntil - millis()) > 0) return g_state.overrideFace;
  if (g_state.focusPhase == FocusPhase::Work &&
      (g_state.mood == Face::Neutral || g_state.mood == Face::Focused))
    return Face::Focused;
  return g_state.mood;
}

void showFace(Face f, uint32_t durationMs) {
  g_state.overrideFace = f;
  g_state.overrideUntil = millis() + durationMs;
}
