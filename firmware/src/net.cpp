#include "net.h"
#include "config.h"
#include "state.h"
#include "camera.h"
#include "audio.h"
#include "focus_timer.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Optional compile-time default; normally the server address is set in the Wi-Fi setup portal.
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef SERVER_URL
  #define SERVER_URL ""
#endif

namespace {
// A reply is downloaded completely before it plays, so Wi-Fi hiccups can't cause clicks.
class MemStream : public Stream {
 public:
  MemStream(const uint8_t* d, size_t n) : d_(d), n_(n) {}
  int available() override { return n_ - p_; }
  int read() override { return p_ < n_ ? d_[p_++] : -1; }
  int peek() override { return p_ < n_ ? d_[p_] : -1; }
  size_t readBytes(char* buf, size_t len) override {
    size_t k = min(len, n_ - p_);
    memcpy(buf, d_ + p_, k);
    p_ += k;
    return k;
  }
  size_t write(uint8_t) override { return 0; }
  void flush() override {}
 private:
  const uint8_t* d_;
  size_t n_, p_ = 0;
};

// Read the whole HTTP body into PSRAM. Returns nullptr on failure (caller frees).
uint8_t* downloadBody(HTTPClient& http, size_t& len) {
  int total = http.getSize();
  WiFiClient* s = http.getStreamPtr();
  size_t cap = total > 0 ? (size_t)total : 256 * 1024;
  uint8_t* buf = (uint8_t*)ps_malloc(cap);
  if (!buf) return nullptr;
  len = 0;
  uint32_t last = millis();
  while ((total < 0 || len < (size_t)total) && millis() - last < 4000) {
    size_t avail = s->available();
    if (avail) {
      if (len + avail > cap) {
        if (total > 0) break;
        cap *= 2;
        uint8_t* bigger = (uint8_t*)ps_realloc(buf, cap);
        if (!bigger) break;
        buf = bigger;
      }
      len += s->readBytes(buf + len, min(avail, cap - len));
      last = millis();
    } else if (!s->connected()) {
      break;
    } else {
      delay(2);
    }
  }
  return buf;
}

bool playDownloaded(HTTPClient& http) {
  size_t len = 0;
  uint8_t* buf = downloadBody(http, len);
  if (!buf || len < 44) { free(buf); return false; }
  MemStream ms(buf, len);
  bool ok = Audio::playWavStream(&ms, len);
  free(buf);
  return ok;
}

QueueHandle_t s_queue = nullptr;
SemaphoreHandle_t s_urlLock = nullptr;
String s_url;

String fullUrl(const char* path) {
  if (!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8)) return String(path);
  return Net::serverUrl() + path;
}

String urlDecode(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '%' && i + 2 < in.length()) {
      char hex[3] = {in[i + 1], in[i + 2], 0};
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else if (c == '+') out += ' ';
    else out += c;
  }
  return out;
}

void addTelemetryHeaders(HTTPClient& http) {
  if (!isnan(g_state.temperature)) http.addHeader("X-Temp", String((float)g_state.temperature, 1));
  if (!isnan(g_state.humidity))    http.addHeader("X-Hum", String((float)g_state.humidity, 1));
  http.addHeader("X-Focus", focusPhaseName(g_state.focusPhase));
  http.addHeader("X-Focus-Left", String((uint32_t)g_state.focusRemainingSec));
  http.addHeader("X-Focus-Total", String((uint32_t)g_state.focusTotalSec));
  http.addHeader("X-Focus-Work", String(FocusTimer::workMinutes()));
  http.addHeader("X-Focus-Break", String(FocusTimer::breakMinutes()));
  http.addHeader("X-Sessions", String((uint16_t)g_state.focusSessions));
  http.addHeader("X-Busy", g_state.activity != Activity::Idle ? "1" : "0");
  http.addHeader("X-Volume", String((uint8_t)g_state.volume));
  http.addHeader("X-Face", faceName(currentFace()));
  http.addHeader("X-Camera", g_state.cameraOk ? "1" : "0");
  http.addHeader("X-Ip", WiFi.localIP().toString());
  http.addHeader("X-Wifi", WiFi.SSID());
  http.addHeader("X-Rssi", String(WiFi.RSSI()));
}

void parseServerReply(const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  if (doc["sfx"].is<bool>()) g_state.sfx = doc["sfx"].as<bool>();
  if (doc["listen"].is<bool>()) g_state.autoListen = doc["listen"].as<bool>();
  if (doc["ai"].is<const char*>()) strlcpy(g_state.aiState, doc["ai"].as<const char*>(), sizeof(g_state.aiState));
  if (doc["mood"].is<const char*>()) g_state.mood = faceFromName(doc["mood"].as<const char*>(), g_state.mood);
  for (JsonObject c : doc["cmds"].as<JsonArray>()) {
    BotCmd cmd = {};
    strlcpy(cmd.type, c["t"] | "", sizeof(cmd.type));
    if (c["v"].is<const char*>()) strlcpy(cmd.v, c["v"].as<const char*>(), sizeof(cmd.v));
    else if (!c["v"].isNull()) snprintf(cmd.v, sizeof(cmd.v), "%ld", (long)(c["v"] | 0));
    strlcpy(cmd.text, c["text"] | "", sizeof(cmd.text));
    cmd.a = c["a"] | 0;
    cmd.b = c["b"] | 0;
    Net::pushCmd(cmd);
  }
}

void writeWavHeader(uint8_t* h, uint32_t dataLen, uint32_t rate) {
  auto w32 = [&](int off, uint32_t v) { h[off] = v; h[off + 1] = v >> 8; h[off + 2] = v >> 16; h[off + 3] = v >> 24; };
  auto w16 = [&](int off, uint16_t v) { h[off] = v; h[off + 1] = v >> 8; };
  memcpy(h, "RIFF", 4);  w32(4, 36 + dataLen);
  memcpy(h + 8, "WAVEfmt ", 8);
  w32(16, 16); w16(20, 1); w16(22, 1);
  w32(24, rate); w32(28, rate * 2); w16(32, 2); w16(34, 16);
  memcpy(h + 36, "data", 4); w32(40, dataLen);
}

// Sends a camera frame + telemetry every FRAME_INTERVAL_MS and collects
// commands (speech, faces, beeps, focus control) from the brain server.
void uploadTask(void*) {
  HTTPClient http;
  uint32_t lastReconnect = 0;
  for (;;) {
    uint32_t t0 = millis();
    g_state.wifiOk = WiFi.status() == WL_CONNECTED;
    if (!g_state.wifiOk) {
      g_state.serverOk = false;
      if (millis() - lastReconnect > 10000) { WiFi.reconnect(); lastReconnect = millis(); }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // While the bot is speaking, skip camera uploads: keeps Wi-Fi and the CPU free for audio
    if (g_state.audioBusy || g_state.activity == Activity::Speaking) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    String events = takeEvents();
    camera_fb_t* fb = g_state.cameraOk ? Camera::grab() : nullptr;

    http.begin(fullUrl("/api/device/frame"));
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", fb ? "image/jpeg" : "application/octet-stream");
    addTelemetryHeaders(http);
    if (events.length()) http.addHeader("X-Event", events);

    int code = fb ? http.POST(fb->buf, fb->len) : http.POST((uint8_t*)"", 0);
    Camera::release(fb);

    if (code == 200) {
      g_state.serverOk = true;
      g_state.lastServerOkMs = millis();
      parseServerReply(http.getString());
    } else {
      g_state.serverOk = false;
      if (events.length()) pushEvent(events.c_str());   // retry next time
      Serial.printf("[net] frame upload failed: %d\n", code);
    }
    http.end();

    uint32_t spent = millis() - t0;
    uint32_t wait = code == 200 ? FRAME_INTERVAL_MS : 3000;
    if (spent < wait) vTaskDelay(pdMS_TO_TICKS(wait - spent));
  }
}
}  // namespace

void Net::begin() {
  s_queue = xQueueCreate(10, sizeof(BotCmd));
  s_urlLock = xSemaphoreCreateMutex();
  Preferences p;
  p.begin("deskbot", false);
  bool noDefaults = p.getBool("nodefaults", false);   // set by a factory reset
  s_url = p.isKey("server") ? p.getString("server", SERVER_URL) : String(noDefaults ? "" : SERVER_URL);
  p.end();
  Serial.printf("[net] brain server: %s\n", s_url.c_str());
  xTaskCreatePinnedToCore(uploadTask, "upload", 8192, nullptr, 1, nullptr, 0);
}

String Net::serverUrl() {
  xSemaphoreTake(s_urlLock, portMAX_DELAY);
  String u = s_url;
  xSemaphoreGive(s_urlLock);
  return u;
}

void Net::setServerUrl(const String& url) {
  String u = url;
  u.trim();
  while (u.endsWith("/")) u.remove(u.length() - 1);
  if (!u.startsWith("http")) u = "http://" + u;
  xSemaphoreTake(s_urlLock, portMAX_DELAY);
  s_url = u;
  xSemaphoreGive(s_urlLock);
  Preferences p;
  p.begin("deskbot", false);
  p.putString("server", u);
  p.end();
}

bool Net::pushCmd(const BotCmd& c) { return s_queue && xQueueSend(s_queue, &c, 0) == pdTRUE; }
bool Net::popCmd(BotCmd& c) { return s_queue && xQueueReceive(s_queue, &c, 0) == pdTRUE; }

bool Net::voiceRoundTrip(uint8_t* buf, size_t samples, bool autoMode) {
  uint32_t dataLen = samples * 2;
  writeWavHeader(buf, dataLen, AUDIO_SAMPLE_RATE);

  HTTPClient http;
  http.begin(fullUrl(autoMode ? "/api/device/voice?auto=1" : "/api/device/voice"));
  http.setTimeout(45000);
  http.addHeader("Content-Type", "audio/wav");
  addTelemetryHeaders(http);
  const char* keys[] = {"X-Reply-Text", "X-User-Text", "X-Face"};
  http.collectHeaders(keys, 3);

  int code = http.POST(buf, 44 + dataLen);
  if (code == 204) {               // hands-free: not meant for the bot
    http.end();
    return true;
  }
  if (code != 200) {
    Serial.printf("[net] voice failed: %d\n", code);
    http.end();
    return false;
  }
  String reply = urlDecode(http.header("X-Reply-Text"));
  String face = http.header("X-Face");
  Serial.printf("[voice] you: %s\n[voice] bot: %s\n", urlDecode(http.header("X-User-Text")).c_str(), reply.c_str());

  if (face.length()) showFace(faceFromName(face.c_str()), 4000 + reply.length() * 70);
  if (reply.length()) setCaption(reply, 4000 + reply.length() * 70);

  g_state.activity = Activity::Speaking;
  bool ok = playDownloaded(http);
  g_state.activity = Activity::Idle;
  http.end();
  return ok;
}

bool Net::playFromServer(const char* path) {
  HTTPClient http;
  http.begin(fullUrl(path));
  http.setTimeout(20000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[net] audio GET %s failed: %d\n", path, code);
    http.end();
    return false;
  }
  g_state.activity = Activity::Speaking;
  bool ok = playDownloaded(http);
  g_state.activity = Activity::Idle;
  http.end();
  return ok;
}
