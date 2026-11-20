#include "button.h"
#include "config.h"

namespace {
const uint32_t DEBOUNCE_MS = 25;
const uint32_t LONG_MS = 900;
const uint32_t DOUBLE_GAP_MS = 300;

bool s_stable = false, s_lastRaw = false;
uint32_t s_rawChange = 0, s_downAt = 0, s_upAt = 0;
bool s_longFired = false;
uint8_t s_clicks = 0;
}  // namespace

void Button::begin() { pinMode(BUTTON_PIN, INPUT_PULLUP); }

bool Button::isDown() { return digitalRead(BUTTON_PIN) == LOW; }

void Button::clear() {
  s_clicks = 0;
  s_stable = s_lastRaw = isDown();
  s_longFired = true;      // a press still held now must not become a long press
  s_rawChange = millis();
}

ButtonEvent Button::poll() {
  uint32_t now = millis();
  bool raw = digitalRead(BUTTON_PIN) == LOW;
  if (raw != s_lastRaw) { s_lastRaw = raw; s_rawChange = now; }

  if (now - s_rawChange > DEBOUNCE_MS && raw != s_stable) {
    s_stable = raw;
    if (s_stable) {                // pressed
      s_downAt = now;
      s_longFired = false;
    } else {                       // released
      s_upAt = now;
      if (!s_longFired) s_clicks++;
    }
  }

  if (s_stable && !s_longFired && now - s_downAt > LONG_MS) {
    s_longFired = true;
    s_clicks = 0;
    return ButtonEvent::LongPress;
  }

  if (!s_stable && s_clicks > 0 && now - s_upAt > DOUBLE_GAP_MS) {
    uint8_t c = s_clicks;
    s_clicks = 0;
    return c >= 2 ? ButtonEvent::DoubleClick : ButtonEvent::Click;
  }
  return ButtonEvent::None;
}
