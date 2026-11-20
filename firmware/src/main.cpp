// =====================================================================
//  DeskBot - ESP32-S3 CAM desk companion
//  Emotion-aware voice assistant + focus timer + phone-use warnings
//  + animated TFT face + AHT10 room climate + web dashboard.
//
//  Heavy AI (face emotion, phone detection, speech-to-text, LLM, TTS)
//  runs on the "brain server" (server/app.py) on your PC.
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>

#include "config.h"
#include "state.h"
#include "display.h"
#include "camera.h"
#include "audio.h"
#include "aht10.h"
#include "buzzer.h"
#include "button.h"
#include "focus_timer.h"
#include "net.h"
#include "web_local.h"
#include "sfx.h"
#include "wifi_setup.h"


static const size_t REC_MAX_SAMPLES = AUDIO_SAMPLE_RATE * RECORD_MAX_SEC;
static uint8_t* s_recBuf = nullptr;      // 44-byte WAV header + PCM, in PSRAM
static uint8_t* s_listenBuf = nullptr;   // same, for hands-free listening
static const size_t LISTEN_MAX_SAMPLES = AUDIO_SAMPLE_RATE * LISTEN_MAX_SEC;
static bool s_audioOk = false;

// ---------------------------------------------------------------------
static void saveVolume(uint8_t v) {
  Preferences p;
  p.begin("deskbot", false);
  p.putUChar("vol", v);
  p.end();
}

static void startFocus(uint16_t work, uint16_t brk) {
  FocusTimer::start(work, brk);
  Buzzer::play("start");
  showFace(Face::Focused, 3000);
  setCaption(String("Focus ") + FocusTimer::workMinutes() + " min. Phone away!", 4000);
  pushEvent("focus_start");
}

static void stopFocus() {
  FocusTimer::stop();
  Buzzer::play("double");
  setCaption("Focus stopped", 2500);
  pushEvent("focus_stop");
}

static void toggleFocusPause() {
  FocusTimer::togglePause();
  bool paused = g_state.focusPhase == FocusPhase::Paused;
  Buzzer::play("beep");
  setCaption(paused ? "Paused" : "Resumed", 2000);
  pushEvent(paused ? "focus_pause" : "focus_resume");
}

// ---------------------------------------------------------------------
//  Voice assistant: click -> listen -> server (STT + LLM + TTS) -> speak
// ---------------------------------------------------------------------
// Erase everything (Wi-Fi, server, volume...) and restart into DeskBot-Setup.
static void factoryReset() {
  g_state.resetCountdown = 0;
  Buzzer::toneBlocking(880, 150);
  Buzzer::toneBlocking(440, 300);
  Preferences p;
  p.begin("deskbot", false);
  p.clear();
  p.putBool("nodefaults", true);      // don't fall back to compiled-in Wi-Fi either
  p.end();
  WiFi.disconnect(true, true);        // also erase the Wi-Fi stored by the ESP32 itself
  delay(700);
  ESP.restart();
}

// Called when a long press starts on the Info screen: keep holding to reset.
static void resetHold() {
  uint32_t t0 = millis() - 900;       // the long press already lasted ~0.9 s
  int8_t shown = -1;
  while (Button::isDown()) {
    uint32_t held = millis() - t0;
    if (held >= RESET_HOLD_MS) factoryReset();
    int8_t left = (RESET_HOLD_MS - held + 999) / 1000;
    if (left != shown) {
      shown = left;
      g_state.resetCountdown = left;
      Buzzer::toneBlocking(1400, 35);
    }
    delay(20);
  }
  g_state.resetCountdown = -1;
  setCaption("Reset cancelled", 2000);
  Button::clear();
}

// Hands-free: always listening; the server answers only when the bot is called by name.
static bool listenShouldPause() {
  return !g_state.autoListen || !g_state.serverOk || !s_audioOk ||
         g_state.activity != Activity::Idle || g_state.audioBusy ||
         (int32_t)(g_state.audioQuietUntil - millis()) > 0 || Button::isDown() ||
         g_state.resetCountdown >= 0;
}

static void listenTask(void*) {
  for (;;) {
    if (listenShouldPause()) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    size_t n = Audio::listen((int16_t*)(s_listenBuf + 44), LISTEN_MAX_SAMPLES, listenShouldPause);
    if (n < AUDIO_SAMPLE_RATE * 4 / 10) continue;          // shorter than 0.4 s: a knock or click
    g_state.activity = Activity::Listening;                // "I heard something"
    Net::voiceRoundTrip(s_listenBuf, n, true);             // speaks only if it was for the bot
    if (g_state.activity == Activity::Listening) g_state.activity = Activity::Idle;
  }
}

static void voiceInteraction() {
#if !MIC_ENABLED
  Buzzer::play("double");
  showFace(Face::Happy, 2500);
  setCaption("Mic is off - chat with me on the dashboard!", 4000);
  return;
#endif
  if (!s_audioOk || !s_recBuf) { Buzzer::play("error"); setCaption("Audio not available", 3000); return; }
  if (!g_state.serverOk) {
    Buzzer::play("error");
    showFace(Face::Confused, 3000);
    setCaption("Brain server is offline", 4000);
    return;
  }

  g_state.activity = Activity::Listening;
  setCaption("Listening...", 10000);
  Buzzer::toneBlocking(1760, 50);
  delay(60);                                   // don't record our own beep

  bool released = !Button::isDown();
  auto stopFn = [&]() {                        // a second press stops recording
    bool down = Button::isDown();
    if (!down) released = true;
    return released && down;
  };
  size_t n = Audio::record((int16_t*)(s_recBuf + 44), REC_MAX_SAMPLES, stopFn);
  while (Button::isDown()) delay(5);

  if (n < AUDIO_SAMPLE_RATE * 3 / 10) {
    g_state.activity = Activity::Idle;
    showFace(Face::Confused, 2500);
    setCaption("I didn't hear anything", 3000);
    Button::clear();
    return;
  }

  g_state.activity = Activity::Thinking;
  setCaption("Thinking...", 30000);
  bool ok = Net::voiceRoundTrip(s_recBuf, n);
  g_state.activity = Activity::Idle;
  if (!ok) {
    Buzzer::play("error");
    showFace(Face::Sad, 3000);
    setCaption("Sorry, I couldn't reach my brain", 4000);
  }
  Button::clear();
}

// ---------------------------------------------------------------------
//  Commands from the brain server / local page
// ---------------------------------------------------------------------
static void handleCmd(const BotCmd& c) {
  if (!strcmp(c.type, "say")) {
    if (c.text[0]) setCaption(c.text, 4000 + strlen(c.text) * 70);
    if (c.v[0] && s_audioOk) Net::playFromServer(c.v);
  } else if (!strcmp(c.type, "face")) {
    showFace(faceFromName(c.v), c.a > 0 ? c.a : 4000);
  } else if (!strcmp(c.type, "mood")) {
    g_state.mood = faceFromName(c.v, g_state.mood);
  } else if (!strcmp(c.type, "beep")) {
    Buzzer::play(c.v);
  } else if (!strcmp(c.type, "focus")) {
    if      (!strcmp(c.v, "start"))  startFocus(c.a, c.b);
    else if (!strcmp(c.v, "stop"))   { if (FocusTimer::active()) stopFocus(); }
    else if (!strcmp(c.v, "pause"))  { if (g_state.focusPhase != FocusPhase::Paused && FocusTimer::active()) toggleFocusPause(); }
    else if (!strcmp(c.v, "resume")) { if (g_state.focusPhase == FocusPhase::Paused) toggleFocusPause(); }
    else if (!strcmp(c.v, "toggle")) { if (FocusTimer::active()) toggleFocusPause(); else startFocus(0, 0); }
  } else if (!strcmp(c.type, "vol")) {
    int v = c.a ? c.a : atoi(c.v);
    Audio::setVolume(constrain(v, 0, 100));
    saveVolume(Audio::volume());
  } else if (!strcmp(c.type, "caption")) {
    setCaption(c.text[0] ? c.text : c.v, c.a > 0 ? c.a : 6000);
  } else if (!strcmp(c.type, "server")) {
    // Move to another DeskBot server - only after checking it answers.
    String url = WifiSetup::normaliseServer(c.v);
    HTTPClient http;
    http.begin(url + "/api/state");
    http.setTimeout(4000);
    int code = http.GET();
    http.end();
    if (code == 200) {
      Net::setServerUrl(url);
      setCaption("Moved to " + url, 5000);
      Buzzer::play("success");
      pushEvent("server_changed");
    } else {
      setCaption("Can't reach " + url + " - keeping the old server", 6000);
      Buzzer::play("error");
      pushEvent("server_change_failed");
    }
  } else if (!strcmp(c.type, "wifisetup")) {
    setCaption("Opening Wi-Fi setup...", 3000);
    Buzzer::toneBlocking(1319, 120);
    delay(800);
    WifiSetup::requestPortalOnNextBoot();
    ESP.restart();
  } else if (!strcmp(c.type, "screen")) {
    g_state.screenMode = !strcmp(c.v, "sensors") ? 1 : !strcmp(c.v, "info") ? 2 : 0;
  }
}

// ---------------------------------------------------------------------
static void connectWifi() {
  // 1) Button held at power-on, or "Change Wi-Fi" pressed on the bot's page -> setup portal
  bool held = false;
  if (Button::isDown()) {
    Display::bootMessage("Wi-Fi setup", "keep holding the button...");
    uint32_t t0 = millis();
    while (Button::isDown() && millis() - t0 < 1500) delay(20);
    held = Button::isDown();
  }
  if (held) WifiSetup::runPortal("", 0);
  // Opened from the dashboard / bot page: give up after 5 minutes and rejoin the saved Wi-Fi
  if (WifiSetup::portalRequested()) WifiSetup::runPortal("", 300000);

  // 2) Nothing saved yet -> setup portal (waits until saved)
  if (!WifiSetup::hasCredentials())
    WifiSetup::runPortal("", 0);

  // 3) Join the saved network; if it fails, open the portal for 3 minutes, then retry
  if (!WifiSetup::connectSaved(20000)) {
    String why = "Couldn't join \"" + WifiSetup::savedSsid() + "\". Check the password or pick another network.";
    WifiSetup::runPortal(why.c_str(), 180000);
  }

  g_state.wifiOk = true;
  Serial.printf("[wifi] %s\n", WiFi.localIP().toString().c_str());
  MDNS.begin(MDNS_NAME);
  MDNS.addService("http", "tcp", 80);
  Display::bootMessage("Online", WiFi.localIP().toString().c_str());
  delay(800);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== DeskBot booting ===");

  Buzzer::begin();
  Button::begin();

  if (!Display::begin()) Serial.println("[tft] init failed (check driver in config.h)");
  Display::bootMessage("DeskBot", "starting...");

  // Camera FIRST: it configures the sensor over SCCB (GPIO4/5) before
  // the I2S mic claims GPIO5.
  g_state.cameraOk = Camera::begin();

  s_recBuf = (uint8_t*)ps_malloc(44 + REC_MAX_SAMPLES * 2);
  s_listenBuf = (uint8_t*)ps_malloc(44 + LISTEN_MAX_SAMPLES * 2);
  s_audioOk = Audio::begin();
  Preferences p;
  p.begin("deskbot", false);
  Audio::setVolume(p.getUChar("vol", DEFAULT_VOLUME));
  p.end();

  g_state.sensorOk = AHT10::begin(AHT_SDA_PIN, AHT_SCL_PIN);
  if (!g_state.sensorOk) Serial.println("[aht10] not found");

  connectWifi();
  Net::begin();
  WebLocal::begin();
  Display::startTask();
  if (MIC_ENABLED && s_audioOk && s_listenBuf)
    xTaskCreatePinnedToCore(listenTask, "listen", 8192, nullptr, 1, nullptr, 1);

  Buzzer::play("start");
  showFace(Face::Happy, 3000);
  setCaption(MIC_ENABLED ? "Hi! Click to talk, hold for focus mode." : "Hi! Hold the button for focus mode.", 5000);
  pushEvent("boot");
}

void loop() {
  static uint32_t lastSensor = 0;

  switch (Button::poll()) {
    case ButtonEvent::Click:
      if (FocusTimer::active()) toggleFocusPause();   // pause / resume a running session
      else voiceInteraction();
      break;
    case ButtonEvent::LongPress:
      if (g_state.screenMode == 2) resetHold();      // Info screen: hold 5 s to reset
      else if (FocusTimer::active()) stopFocus();
      else startFocus(0, 0);
      break;
    case ButtonEvent::DoubleClick:                   // face -> room -> info -> face
      g_state.screenMode = (g_state.screenMode + 1) % 3;
      Buzzer::play("beep");
      break;
    default: break;
  }

  switch (FocusTimer::tick()) {
    case FocusEvent::WorkDone:
      Buzzer::play("success");
      showFace(Face::Happy, 6000);
      setCaption("Great work! Take a break.", 5000);
      pushEvent("work_done");
      break;
    case FocusEvent::BreakDone:
      Buzzer::play("break");
      showFace(Face::Surprised, 3000);
      setCaption("Break over - back to focus!", 5000);
      pushEvent("break_done");
      break;
    default: break;
  }

  if (millis() - lastSensor > SENSOR_PERIOD_MS) {
    lastSensor = millis();
    float t, h;
    if (g_state.sensorOk && AHT10::read(t, h)) {
      g_state.temperature = t;
      g_state.humidity = h;
    }
  }

  BotCmd cmd;
  while (Net::popCmd(cmd)) handleCmd(cmd);

  Sfx::update(s_audioOk);
  Buzzer::update();
  WebLocal::handle();
  delay(5);
}
