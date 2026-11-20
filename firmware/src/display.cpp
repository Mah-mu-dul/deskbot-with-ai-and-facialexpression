#include "display.h"
#include "config.h"
#include "state.h"
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include "net.h"

// ---------------------------------------------------------------------
//  Panel selection
// ---------------------------------------------------------------------
static Arduino_DataBus* bus =
    new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI);

#if defined(DISPLAY_ILI9488_480x320)
  #define PANEL_W 320
  #define PANEL_H 480
  static Arduino_GFX* panel = new Arduino_ILI9488_18bit(bus, TFT_RST, TFT_ROTATION, false);
#elif defined(DISPLAY_ST7796_480x320)
  #define PANEL_W 320
  #define PANEL_H 480
  static Arduino_GFX* panel = new Arduino_ST7796(bus, TFT_RST, TFT_ROTATION, false, 320, 480);
#elif defined(DISPLAY_ST7789_240x240)
  #define PANEL_W 240
  #define PANEL_H 240
  static Arduino_GFX* panel = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 240, 240, 0, 0, 0, 80);
#elif defined(DISPLAY_ST7789_240x320)
  #define PANEL_W 240
  #define PANEL_H 320
  static Arduino_GFX* panel = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 240, 320);
#elif defined(DISPLAY_ST7789_135x240)
  #define PANEL_W 135
  #define PANEL_H 240
  static Arduino_GFX* panel = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true, 135, 240, 52, 40, 53, 40);
#elif defined(DISPLAY_ILI9341_240x320)
  #define PANEL_W 240
  #define PANEL_H 320
  static Arduino_GFX* panel = new Arduino_ILI9341(bus, TFT_RST, TFT_ROTATION, false);
#elif defined(DISPLAY_ST7735_128x160)
  #define PANEL_W 128
  #define PANEL_H 160
  static Arduino_GFX* panel = new Arduino_ST7735(bus, TFT_RST, TFT_ROTATION, false, 128, 160, 0, 0, 0, 0, false);
#else
  #error "Select a display driver in config.h"
#endif

#if (TFT_ROTATION % 2) == 1
  static const int16_t W = PANEL_H, H = PANEL_W;
#else
  static const int16_t W = PANEL_W, H = PANEL_H;
#endif

static Arduino_Canvas* canvas = nullptr;

// ---------------------------------------------------------------------
//  Colours (RGB565)
// ---------------------------------------------------------------------
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static const uint16_t BG      = rgb(0, 0, 0);
static const uint16_t C_CYAN  = rgb(40, 220, 255);
static const uint16_t C_GREEN = rgb(80, 255, 160);
static const uint16_t C_BLUE  = rgb(70, 130, 255);
static const uint16_t C_RED   = rgb(255, 50, 50);
static const uint16_t C_YELL  = rgb(255, 230, 90);
static const uint16_t C_PINK  = rgb(255, 90, 170);
static const uint16_t C_PURP  = rgb(180, 120, 255);
static const uint16_t C_ORNG  = rgb(255, 150, 40);
static const uint16_t C_DIM   = rgb(30, 110, 140);
static const uint16_t C_GREY  = rgb(120, 120, 120);
static const uint16_t C_WHITE = rgb(255, 255, 255);
static const uint16_t C_PANEL = rgb(20, 24, 32);

// ---------------------------------------------------------------------
//  Expression definitions
// ---------------------------------------------------------------------
struct EyeShape {
  float w, h, r;         // size & corner radius (240px reference)
  float lidTop;          // 0..1 fraction covered from the top
  float lidAngle;        // + = inner corner lower (angry), - = outer lower (sad)
  float lidBottom;       // 0..1 happy "smile" cut from below
  float dy;              // vertical offset
  uint16_t color;
};

static EyeShape shapeFor(Face f) {
  switch (f) {
    case Face::Happy:     return {60, 58, 22, 0.00f,  0.00f, 0.45f,  0, C_GREEN};
    case Face::Sad:       return {54, 60, 16, 0.18f, -0.40f, 0.00f, 10, C_BLUE};
    case Face::Angry:     return {58, 56, 12, 0.12f,  0.50f, 0.00f,  4, C_RED};
    case Face::Warning:   return {58, 56, 12, 0.12f,  0.50f, 0.00f,  4, C_RED};
    case Face::Surprised: return {62, 80, 31, 0.00f,  0.00f, 0.00f, -4, C_YELL};
    case Face::Sleepy:    return {58, 70, 18, 0.62f,  0.00f, 0.00f, 10, C_DIM};
    case Face::Love:      return {60, 60, 20, 0.00f,  0.00f, 0.00f,  0, C_PINK};
    case Face::Focused:   return {60, 64, 14, 0.36f,  0.00f, 0.00f,  4, C_CYAN};
    case Face::Listening: return {62, 76, 28, 0.00f,  0.00f, 0.00f, -4, C_CYAN};
    case Face::Thinking:  return {54, 62, 18, 0.10f, -0.10f, 0.20f,  0, C_PURP};
    case Face::Confused:  return {56, 66, 18, 0.00f,  0.00f, 0.00f,  0, C_ORNG};
    case Face::Speaking:  return {58, 68, 20, 0.00f,  0.00f, 0.15f,  0, C_CYAN};
    default:              return {56, 70, 18, 0.00f,  0.00f, 0.00f,  0, C_CYAN};
  }
}

// Animated (interpolated) state
static EyeShape cur = shapeFor(Face::Neutral);
static float lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
static uint32_t nextLook = 0, nextBlink = 0, blinkStart = 0;
static bool blinking = false;
static float S = 1.0f;          // scale vs 240px reference
static int16_t CX, CY, EYE_DX, MOUTH_Y;

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------------
//  Drawing primitives
// ---------------------------------------------------------------------
static void drawHeart(int16_t cx, int16_t cy, int16_t size, uint16_t c) {
  int16_t r = size / 4;
  canvas->fillCircle(cx - r, cy - r / 2, r, c);
  canvas->fillCircle(cx + r, cy - r / 2, r, c);
  canvas->fillTriangle(cx - 2 * r, cy - r / 4, cx + 2 * r, cy - r / 4, cx, cy + 2 * r, c);
}

static void drawEye(int16_t cx, int16_t cy, const EyeShape& e, bool rightEye, float blink, float hMul) {
  int16_t w = e.w * S;
  int16_t h = max(4.0f, e.h * S * blink * hMul);
  int16_t x = cx - w / 2, y = cy - h / 2;
  int16_t r = min<int16_t>(e.r * S, min(w, h) / 2);
  canvas->fillRoundRect(x, y, w, h, r, e.color);

  // Top eyelid (slanted line, everything above is erased)
  if (e.lidTop > 0.01f || fabsf(e.lidAngle) > 0.01f) {
    float base = y + h * e.lidTop;
    float slope = e.lidAngle * h;
    float inner = base + slope / 2, outer = base - slope / 2;
    float yl = rightEye ? inner : outer;   // left edge of this eye
    float yr = rightEye ? outer : inner;   // right edge
    int16_t top = y - 2;
    canvas->fillTriangle(x - 2, top, x + w + 2, top, x - 2, (int16_t)yl, BG);
    canvas->fillTriangle(x + w + 2, top, x + w + 2, (int16_t)yr, x - 2, (int16_t)yl, BG);
  }
  // Bottom cut -> happy "^" eyes
  if (e.lidBottom > 0.01f) {
    int16_t ty = y + h * (1.0f - e.lidBottom);
    int16_t ry = h * 0.6f, rx = w * 0.75f;
    canvas->fillEllipse(cx, ty + ry, rx, ry, BG);
  }
}

static void drawMouth(Face f, uint32_t now) {
  int16_t mx = CX, my = MOUTH_Y;
  uint16_t col = cur.color;
  switch (f) {
    case Face::Happy:
    case Face::Love:
      canvas->fillArc(mx, my - 12 * S, 22 * S, 17 * S, 20, 160, col);
      break;
    case Face::Sad:
      canvas->fillArc(mx, my + 14 * S, 20 * S, 16 * S, 205, 335, col);
      break;
    case Face::Angry:
    case Face::Warning:
      canvas->fillRoundRect(mx - 18 * S, my, 36 * S, 5 * S, 2, col);
      break;
    case Face::Surprised:
      canvas->fillEllipse(mx, my + 4 * S, 11 * S, 13 * S, col);
      canvas->fillEllipse(mx, my + 4 * S, 6 * S, 8 * S, BG);
      break;
    case Face::Sleepy: {
      canvas->fillEllipse(mx, my + 2 * S, 7 * S, 4 * S, col);
      int k = (now / 600) % 3;
      canvas->setTextColor(C_GREY);
      for (int i = 0; i <= k; i++) {
        canvas->setTextSize(1 + i);
        canvas->setCursor(CX + 60 * S + i * 14 * S, CY - 50 * S - i * 16 * S);
        canvas->print("z");
      }
      break;
    }
    case Face::Speaking: {
      float lvl = g_state.speakLevel / 255.0f;
      int16_t mh = (3 + lvl * 22) * S;
      canvas->fillRoundRect(mx - 16 * S, my - mh / 2 + 4 * S, 32 * S, mh, min<int16_t>(8 * S, mh / 2), col);
      break;
    }
    case Face::Listening: {
      float lvl = g_state.speakLevel / 255.0f;
      for (int i = -3; i <= 3; i++) {
        float wob = 0.35f + 0.65f * fabsf(sinf(now / 120.0f + i * 0.9f));
        int16_t bh = (4 + lvl * 26 * wob + 3 * wob) * S;
        canvas->fillRoundRect(mx + i * 9 * S - 3 * S, my + 4 * S - bh / 2, 6 * S, bh, 2, col);
      }
      break;
    }
    case Face::Thinking: {
      int k = (now / 300) % 4;
      for (int i = 0; i < 3; i++)
        canvas->fillCircle(mx - 14 * S + i * 14 * S, my + 4 * S, (i < k ? 5 : 3) * S, i < k ? col : C_GREY);
      break;
    }
    case Face::Confused:
      for (int i = 0; i < 4; i++)
        canvas->drawLine(mx - 20 * S + i * 10 * S, my + ((i & 1) ? 6 : 0) * S,
                         mx - 10 * S + i * 10 * S, my + ((i & 1) ? 0 : 6) * S, col);
      break;
    case Face::Focused:
      canvas->fillRoundRect(mx - 12 * S, my + 2 * S, 24 * S, 4 * S, 2, col);
      break;
    default:
      canvas->fillRoundRect(mx - 15 * S, my + 2 * S, 30 * S, 4 * S, 2, col);
      break;
  }
}

// Usable area inside the enclosure opening (SCREEN_PAD_* in config.h)
static const int16_t PX = SCREEN_PAD_X, PY = SCREEN_PAD_Y;
static const int16_t IW = W - 2 * SCREEN_PAD_X, IH = H - 2 * SCREEN_PAD_Y;
static uint8_t TS = 1;        // base text size (2 on big screens)

static void drawStatusBar() {
  canvas->setTextSize(TS);
  canvas->setTextColor(C_GREY);
  canvas->setCursor(PX + 4, PY + 4);
  if (!isnan(g_state.temperature))
    canvas->printf("%.1fC  %.0f%%", (float)g_state.temperature, (float)g_state.humidity);
  else
    canvas->print("--.-C  --%");
  // connectivity dots: wifi, server, camera
  int16_t r = 3 * TS, step = 9 * TS;
  int16_t x = PX + IW - r - 4, y = PY + 4 + 4 * TS;
  canvas->fillCircle(x, y, r, g_state.cameraOk ? C_GREEN : C_RED);  x -= step;
  canvas->fillCircle(x, y, r, g_state.serverOk ? C_GREEN : C_RED);  x -= step;
  canvas->fillCircle(x, y, r, g_state.wifiOk ? C_GREEN : C_RED);
}

static void drawFocusBar() {
  FocusPhase ph = g_state.focusPhase;
  if (ph == FocusPhase::Off) return;
  uint32_t left = g_state.focusRemainingSec, total = max<uint32_t>(1u, (uint32_t)g_state.focusTotalSec);
  char buf[24];
  const char* label = ph == FocusPhase::Work ? "FOCUS" : ph == FocusPhase::Break ? "BREAK" : "PAUSE";
  snprintf(buf, sizeof(buf), "%s %02lu:%02lu", label, (unsigned long)(left / 60), (unsigned long)(left % 60));
  uint8_t ts = TS + 1;
  int16_t tw = strlen(buf) * 6 * ts;
  uint16_t c = ph == FocusPhase::Work ? C_CYAN : ph == FocusPhase::Break ? C_GREEN : C_YELL;
  canvas->setTextSize(ts);
  canvas->setTextColor(c);
  canvas->setCursor(PX + (IW - tw) / 2, PY + IH - 8 * ts - 12);
  canvas->print(buf);
  float frac = 1.0f - (float)left / total;
  canvas->fillRoundRect(PX, PY + IH - 5, IW, 5, 2, C_PANEL);
  canvas->fillRoundRect(PX, PY + IH - 5, (int16_t)(IW * frac), 5, 2, c);
}

static void drawCaption() {
  char text[160];
  if (!getCaption(text, sizeof(text))) return;
  const int cw = 6 * TS, lh = 10 * TS;
  const int perLine = min(63, (IW - 12) / cw);
  int len = strlen(text);
  int lines = min(4, (len + perLine - 1) / perLine);
  int16_t boxH = lines * lh + 8;
  int16_t y0 = PY + IH - boxH - (g_state.focusPhase != FocusPhase::Off ? 16 + 8 * (TS + 1) : 2);
  canvas->fillRoundRect(PX, y0, IW, boxH, 6, C_PANEL);
  canvas->setTextSize(TS);
  canvas->setTextColor(C_WHITE);
  for (int l = 0; l < lines; l++) {
    char line[64];
    int n = min(perLine, len - l * perLine);
    if (n <= 0) break;
    memcpy(line, text + l * perLine, n);
    line[n] = 0;
    canvas->setCursor(PX + 6, y0 + 5 + l * lh);
    canvas->print(line);
  }
}

// Rounded frame around the usable area (inside the enclosure opening)
static void drawFrame() {
  canvas->drawRoundRect(PX - 12, PY - 10, IW + 24, IH + 20, SCREEN_FRAME_R, C_PANEL);
}

// Three dots at the top centre: which screen is showing (face, room, info)
static void drawPageDots() {
  int16_t r = 3 * TS, gap = 10 * TS, cx = PX + IW / 2, y = PY + 4 + 4 * TS;
  for (int i = 0; i < 3; i++) {
    int16_t x = cx + (i - 1) * gap;
    if (i == g_state.screenMode) canvas->fillCircle(x, y, r, C_CYAN);
    else canvas->drawCircle(x, y, r, C_GREY);
  }
}

static void infoRow(int16_t& y, const char* label, const String& value, uint16_t color) {
  canvas->setTextSize(TS);
  canvas->setTextColor(C_GREY);
  canvas->setCursor(PX + 8, y);
  canvas->print(label);
  canvas->setTextColor(color);
  canvas->setCursor(PX + 8 + 9 * 6 * TS, y);
  canvas->print(value);
  y += 12 * TS;
}

static String ago(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60) return String(s) + "s ago";
  if (s < 3600) return String(s / 60) + "m ago";
  return String(s / 3600) + "h ago";
}

static void drawInfoScreen() {
  int16_t y = PY + 16 * TS;
  canvas->setTextSize(TS + 1);
  canvas->setTextColor(C_WHITE);
  canvas->setCursor(PX + 8, y);
  canvas->print("DeskBot info");
  y += 14 * TS;

  uint32_t now = millis();
  bool srv = g_state.serverOk;
  String last = g_state.lastServerOkMs ? ago(now - g_state.lastServerOkMs) : String("never");
  infoRow(y, "Server", srv ? String("Connected") : "Offline, ok " + last, srv ? C_GREEN : C_RED);
  String ai = g_state.aiState;
  bool aiOk = ai == "ok";
  String aiText = aiOk ? "Connected" : ai == "no_key" ? "No API key" : ai == "offline" ? "No internet"
                : ai == "busy" ? "Busy, retrying" : ai == "error" ? "Error" : "Not used yet";
  if (g_state.autoListen && MIC_ENABLED) aiText += "  ears on";
  infoRow(y, "AI", aiText, aiOk ? C_GREEN : (ai == "unknown" ? C_GREY : C_RED));
  String url = Net::serverUrl();
  infoRow(y, "", url.length() ? url : String("not set"), C_GREY);
  bool wifi = WiFi.status() == WL_CONNECTED;
  infoRow(y, "Wi-Fi", wifi ? WiFi.SSID() + "  " + String(WiFi.RSSI()) + " dBm" : String("Not connected"),
          wifi ? C_WHITE : C_RED);
  infoRow(y, "Bot IP", wifi ? WiFi.localIP().toString() : String("-"), C_WHITE);
  String hw = String("Cam ") + (g_state.cameraOk ? "ok" : "off") +
              " Sens " + (g_state.sensorOk ? "ok" : "off") +
              " Mic " + (MIC_ENABLED ? "on" : "off");
  infoRow(y, "Parts", hw, C_WHITE);
  uint32_t up = now / 1000;
  infoRow(y, "Uptime", String(up / 3600) + "h " + String((up / 60) % 60) + "m   v" FW_VERSION, C_WHITE);

  canvas->setTextSize(TS);
  canvas->setTextColor(C_GREY);
  canvas->setCursor(PX + 8, PY + IH - 12 * TS);
  canvas->print("Hold button 5s: reset all");
}

// Full-screen countdown while the button is held for a factory reset
static void drawResetOverlay() {
  int8_t left = g_state.resetCountdown;
  if (left < 0) return;
  int16_t w = IW - 40, h = 70 * TS;
  int16_t x = PX + 20, y = PY + (IH - h) / 2;
  canvas->fillRoundRect(x, y, w, h, 16, C_PANEL);
  canvas->drawRoundRect(x, y, w, h, 16, C_RED);
  canvas->setTextColor(C_RED);
  canvas->setTextSize(TS + 1);
  canvas->setCursor(x + 16, y + 14);
  canvas->print(left > 0 ? "Reset everything?" : "Resetting...");
  canvas->setTextColor(C_WHITE);
  canvas->setTextSize(TS);
  canvas->setCursor(x + 16, y + 14 + 20 * TS);
  if (left > 0) {
    canvas->printf("Keep holding: %d", left);
    canvas->setCursor(x + 16, y + 14 + 32 * TS);
    canvas->setTextColor(C_GREY);
    canvas->print("Release to cancel");
  } else {
    canvas->print("Forgetting Wi-Fi and settings");
  }
}

static void drawSensorScreen() {
  uint8_t big = W >= 400 ? 8 : W >= 200 ? 5 : 3;
  int16_t x = PX + 12;
  canvas->setTextColor(C_GREY);
  canvas->setTextSize(TS);
  canvas->setCursor(x, PY + 14 * TS + 10);
  canvas->print("TEMPERATURE");
  canvas->setTextColor(C_ORNG);
  canvas->setTextSize(big);
  canvas->setCursor(x, PY + 14 * TS + 26);
  if (isnan(g_state.temperature)) canvas->print("--");
  else canvas->printf("%.1fC", (float)g_state.temperature);

  int16_t y2 = PY + IH / 2 + 10;
  canvas->setTextColor(C_GREY);
  canvas->setTextSize(TS);
  canvas->setCursor(x, y2);
  canvas->print("HUMIDITY");
  canvas->setTextColor(C_CYAN);
  canvas->setTextSize(big);
  canvas->setCursor(x, y2 + 16);
  if (isnan(g_state.humidity)) canvas->print("--");
  else canvas->printf("%.0f%%", (float)g_state.humidity);
}

// Send only the rows that changed since the last frame (usually just the eyes):
// far fewer bytes on the wire = smoother animation and fewer chances of a glitch.
// Every 3 s the whole screen is sent again so any glitch heals itself.
static uint16_t* s_prev = nullptr;
static uint32_t s_lastFull = 0;

static void smartFlush() {
  uint16_t* fb = canvas->getFramebuffer();
  if (!fb) { canvas->flush(); return; }
  if (!s_prev) s_prev = (uint16_t*)ps_malloc((size_t)W * H * 2);
  uint32_t now = millis();
  if (!s_prev || now - s_lastFull > 3000) {
    panel->draw16bitRGBBitmap(0, 0, fb, W, H);
    if (s_prev) memcpy(s_prev, fb, (size_t)W * H * 2);
    s_lastFull = now;
    return;
  }
  int16_t y0 = -1, y1 = -1;
  for (int16_t y = 0; y < H; y++) {
    if (memcmp(fb + (size_t)y * W, s_prev + (size_t)y * W, W * 2) != 0) {
      if (y0 < 0) y0 = y;
      y1 = y;
    }
  }
  if (y0 < 0) return;                                     // nothing changed
  int16_t h = y1 - y0 + 1;
  panel->draw16bitRGBBitmap(0, y0, fb + (size_t)y0 * W, W, h);
  memcpy(s_prev + (size_t)y0 * W, fb + (size_t)y0 * W, (size_t)h * W * 2);
}

static void renderFrame() {
  uint32_t now = millis();
  Face f = currentFace();
  canvas->fillScreen(BG);
  drawFrame();
  drawStatusBar();
  drawPageDots();

  if (g_state.activity == Activity::Idle && g_state.screenMode != 0) {
    if (g_state.screenMode == 1) { drawSensorScreen(); drawFocusBar(); }
    else drawInfoScreen();
    drawCaption();
    drawResetOverlay();
    smartFlush();
    return;
  }

  // Interpolate eye geometry toward target expression.
  // While speaking, the eyes keep the current emotion and only the mouth talks.
  Face eyeFace = f;
  if (f == Face::Speaking) {
    Face e = expressionFace();
    if (e != Face::Neutral && e != Face::Focused) eyeFace = e;
  }
  EyeShape t = shapeFor(eyeFace);
  const float k = 0.28f;
  cur.w = lerpf(cur.w, t.w, k);         cur.h = lerpf(cur.h, t.h, k);
  cur.r = lerpf(cur.r, t.r, k);         cur.lidTop = lerpf(cur.lidTop, t.lidTop, k);
  cur.lidAngle = lerpf(cur.lidAngle, t.lidAngle, k);
  cur.lidBottom = lerpf(cur.lidBottom, t.lidBottom, k);
  cur.dy = lerpf(cur.dy, t.dy, k);      cur.color = t.color;

  // Eye movement
  if (now > nextLook) {
    lookTX = random(-14, 15);
    lookTY = random(-8, 9);
    nextLook = now + random(1200, 4000);
  }
  float tx = lookTX, ty = lookTY;
  if (f == Face::Thinking)  { tx = 14; ty = -12; }
  if (f == Face::Focused)   { tx = lookTX * 0.3f; ty = 6; }
  if (f == Face::Listening) { tx = 0; ty = 0; }
  lookX = lerpf(lookX, tx, 0.18f);
  lookY = lerpf(lookY, ty, 0.18f);

  // Blink
  float blink = 1.0f;
  if (!blinking && now > nextBlink && f != Face::Sleepy) { blinking = true; blinkStart = now; }
  if (blinking) {
    float p = (now - blinkStart) / 150.0f;
    if (p >= 1.0f) { blinking = false; nextBlink = now + random(2500, 6000); }
    else blink = 1.0f - sinf(p * PI) * 0.92f;
  }

  int16_t ex = lookX * S, ey = (lookY + cur.dy) * S;
  int16_t lx = CX - EYE_DX + ex, rx = CX + EYE_DX + ex, eyY = CY + ey;

  if (eyeFace == Face::Love) {
    float pulse = 1.0f + 0.08f * sinf(now / 150.0f);
    drawHeart(lx, eyY, 62 * S * pulse, C_PINK);
    drawHeart(rx, eyY, 62 * S * pulse, C_PINK);
  } else {
    drawEye(lx, eyY, cur, false, blink, 1.0f);
    drawEye(rx, eyY, cur, true, blink, eyeFace == Face::Confused ? 0.6f : 1.0f);
  }

  if (f == Face::Warning && (now / 300) % 2 == 0) {
    canvas->fillTriangle(PX + IW - 34, PY + 44, PX + IW - 14, PY + 44, PX + IW - 24, PY + 24, C_YELL);
    canvas->setTextColor(BG); canvas->setTextSize(1);
    canvas->setCursor(PX + IW - 26, PY + 34); canvas->print("!");
  }
  if (f == Face::Sad && (now / 2000) % 3 == 0) {        // occasional tear
    float p = (now % 2000) / 2000.0f;
    canvas->fillCircle(lx + 18 * S, eyY + 30 * S + p * 40 * S, 4 * S, C_BLUE);
  }

  drawMouth(f, now);
  drawFocusBar();
  drawCaption();
  drawResetOverlay();
  smartFlush();
}

static void displayTask(void*) {
  const uint32_t periodMs = 1000 / FACE_FPS;
  for (;;) {
    uint32_t t0 = millis();
    renderFrame();
    uint32_t spent = millis() - t0;
    // Always yield at least 10 ms so the idle task (watchdog) and Wi-Fi get CPU time,
    // even when a large panel takes longer than one frame period to refresh.
    vTaskDelay(pdMS_TO_TICKS(spent < periodMs ? max<uint32_t>(10, periodMs - spent) : 10));
  }
}

bool Display::begin() {
  canvas = new Arduino_Canvas(W, H, panel);
#ifdef GFX_SKIP_OUTPUT_BEGIN
  if (!panel->begin(TFT_SPI_HZ)) return false;
  if (!canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) return false;
#else
  if (!canvas->begin(TFT_SPI_HZ)) return false;
#endif
  TS = W >= 400 ? 2 : 1;
  S = min(IW, IH) / 240.0f;
  if (S < 0.5f) S = 0.5f;
  CX = W / 2;
  CY = PY + IH * 0.45f;
  EYE_DX = 50 * S * (W > H * 1.2f ? 1.4f : 1.0f);   // spread eyes on wide screens
  MOUTH_Y = CY + 62 * S;
  canvas->fillScreen(BG);
  canvas->flush();
  randomSeed(esp_random());
  return true;
}

void Display::bootMessage(const char* l1, const char* l2, const char* l3) {
  if (!canvas) return;
  canvas->fillScreen(BG);
  canvas->setTextColor(C_CYAN);
  canvas->setTextSize(2);
  canvas->setCursor(PX + 10, H / 2 - 20);
  canvas->print(l1);
  if (l2) {
    canvas->setTextSize(1);
    canvas->setTextColor(C_GREY);
    canvas->setCursor(PX + 10, H / 2 + 6);
    canvas->print(l2);
  }
  if (l3) {
    canvas->setTextSize(1);
    canvas->setTextColor(C_GREY);
    canvas->setCursor(PX + 10, H / 2 + 22);
    canvas->print(l3);
  }
  canvas->flush();
}

void Display::startTask() {
  xTaskCreatePinnedToCore(displayTask, "face", 6144, nullptr, 1, nullptr, 0);
}
