#include "audio.h"
#include "config.h"
#include "state.h"
#include "esp_camera.h"
#include <driver/i2s.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <soc/io_mux_reg.h>
#include <soc/gpio_periph.h>
#include <math.h>

namespace {
const i2s_port_t PORT = I2S_NUM_0;
const int CHUNK = 256;                 // stereo frames per I/O block
int32_t s_io[CHUNK * 2];               // [L,R,L,R...] 32-bit slots (speaker)
int32_t s_rx[CHUNK * 2];               // separate buffer for the mic (listening runs in its own task)
SemaphoreHandle_t s_rxLock = nullptr;  // one recorder at a time (button or hands-free)
SemaphoreHandle_t s_txLock = nullptr;  // one player at a time

struct TxGuard {                       // marks the speaker busy so the mic ignores our own voice
  TxGuard() {
    if (s_txLock) xSemaphoreTake(s_txLock, portMAX_DELAY);
    g_state.audioBusy = true;
  }
  ~TxGuard() {
    g_state.audioBusy = false;
    g_state.audioQuietUntil = millis() + 500;
    if (s_txLock) xSemaphoreGive(s_txLock);
  }
};
int s_micSlot = 0;                     // which slot the INMP441 talks in (auto-detected)
uint8_t s_volume = DEFAULT_VOLUME;

void detectMicSlot() {
  size_t br = 0;
  int64_t eL = 0, eR = 0;
  for (int i = 0; i < 20; i++) {
    if (i2s_read(PORT, s_io, sizeof(s_io), &br, 100) != ESP_OK) continue;
    int frames = br / 8;
    for (int f = 0; f < frames; f++) {
      eL += llabs((int64_t)(s_io[f * 2] >> 16));
      eR += llabs((int64_t)(s_io[f * 2 + 1] >> 16));
    }
  }
  // L/R pin tied to GND = left slot. Only switch to right if it is clearly louder
  // (a quiet room makes both slots look the same).
  s_micSlot = (eR > eL * 2 + 1000) ? 1 : 0;
  Serial.printf("[audio] mic slot=%s (L=%lld R=%lld)\n", s_micSlot ? "R" : "L", eL, eR);
}

bool readExact(Stream* s, uint8_t* dst, size_t len) {
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < len && millis() - t0 < 4000) {
    int n = s->readBytes(dst + got, len - got);
    if (n > 0) { got += n; t0 = millis(); }
    else delay(2);
  }
  return got == len;
}

void writeSilence(int ms) {
  memset(s_io, 0, sizeof(s_io));
  size_t bw;
  int blocks = (AUDIO_SAMPLE_RATE * ms / 1000) / CHUNK + 1;
  for (int i = 0; i < blocks; i++) i2s_write(PORT, s_io, sizeof(s_io), &bw, portMAX_DELAY);
}

inline int32_t toSlot(int32_t s16) {
  return (int32_t)((s16 * (int32_t)s_volume) / 100) << 16;
}
}  // namespace

static bool installI2S(bool withTx) {
  i2s_config_t cfg = {};
#if MIC_ENABLED
  cfg.mode = withTx ? (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX)
                    : (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
#else
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);    // speaker only
#endif
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 16;          // more buffering = no clicks if a task is briefly busy
  cfg.dma_buf_len = CHUNK;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;   // output silence when nothing is queued
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[audio] driver install failed");
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_BCLK_PIN;
  pins.ws_io_num = I2S_WS_PIN;
  pins.data_out_num = withTx ? I2S_DOUT_PIN : I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_ENABLED ? I2S_DIN_PIN : I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(PORT, &pins) != ESP_OK) {
    Serial.println("[audio] set pin failed");
    return false;
  }
  i2s_zero_dma_buffer(PORT);
  return true;
}

#if MIC_ENABLED && (I2S_DIN_PIN == CAM_PIN_XCLK || I2S_DIN_PIN == CAM_PIN_PCLK || I2S_DIN_PIN == CAM_PIN_VSYNC)
  #error "I2S_DIN_PIN is a camera pin - use GPIO14 or GPIO21 for the mic"
#endif

bool Audio::begin() {
  s_rxLock = xSemaphoreCreateMutex();
  s_txLock = xSemaphoreCreateMutex();
#if !MIC_ENABLED
  if (!installI2S(true)) return false;
  Serial.println("[audio] speaker only (MIC_ENABLED 0)");
  return true;
#endif
  // If the mic data pin is the camera's SCCB clock (GPIO5 on ESP32-S3 CAM),
  // release the camera's I2C driver so it no longer owns the pin.
  // The camera keeps streaming; only sensor-setting changes stop working.
  if (I2S_DIN_PIN == CAM_PIN_SIOC || I2S_DIN_PIN == CAM_PIN_SIOD) {
    esp_err_t e = i2c_driver_delete(I2C_NUM_1);
    gpio_reset_pin((gpio_num_t)I2S_DIN_PIN);
    gpio_set_pull_mode((gpio_num_t)I2S_DIN_PIN, GPIO_FLOATING);
    Serial.printf("[audio] released SCCB from GPIO%d (i2c delete: %d)\n", I2S_DIN_PIN, e);
  }
  if (!installI2S(true)) return false;
  delay(100);
  detectMicSlot();
  return true;
}

String Audio::micTest(bool rxOnly, bool noCam) {
  if (!MIC_ENABLED) return "{\"mic\":\"disabled\"}";
  if (noCam) {
    esp_camera_deinit();
    g_state.cameraOk = false;
    gpio_reset_pin((gpio_num_t)I2S_DIN_PIN);
    gpio_set_pull_mode((gpio_num_t)I2S_DIN_PIN, GPIO_FLOATING);
  }
  i2s_driver_uninstall(PORT);
  installI2S(!rxOnly);
  delay(200);
  String r = micDebugJson();
  if (rxOnly) {                      // restore normal full-duplex mode
    i2s_driver_uninstall(PORT);
    installI2S(true);
  }
  return r;
}

String Audio::micDebugJson() {
  if (!MIC_ENABLED) return "{\"mic\":\"disabled\"}";
  size_t br = 0;
  int32_t mn[2] = {INT32_MAX, INT32_MAX}, mx[2] = {INT32_MIN, INT32_MIN};
  int64_t sumAbs[2] = {0, 0};
  uint32_t ones[2] = {0, 0}, zeros[2] = {0, 0}, n = 0;
  String first = "";
  for (int i = 0; i < 32; i++) {
    if (i2s_read(PORT, s_io, sizeof(s_io), &br, 100) != ESP_OK || br == 0) continue;
    int frames = br / 8;
    for (int f = 0; f < frames; f++) {
      for (int c = 0; c < 2; c++) {
        int32_t v = s_io[f * 2 + c];
        if (v < mn[c]) mn[c] = v;
        if (v > mx[c]) mx[c] = v;
        sumAbs[c] += llabs((int64_t)(v >> 8));
        if (v == -1) ones[c]++;
        if (v == 0) zeros[c]++;
      }
      if (i == 4 && f < 6) {
        char h[24];
        snprintf(h, sizeof(h), "%08lx/%08lx ", (unsigned long)s_io[f * 2], (unsigned long)s_io[f * 2 + 1]);
        first += h;
      }
      n++;
    }
  }
  // Sample the clock pins: they must toggle (both 0s and 1s) if I2S is running.
  // Enable only the input buffer so the I2S output signal stays routed to the pin.
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[I2S_BCLK_PIN]);
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[I2S_WS_PIN]);
  uint32_t bHigh = 0, wHigh = 0, dHigh = 0;
  const uint32_t SAMPLES = 20000;
  for (uint32_t k = 0; k < SAMPLES; k++) {
    bHigh += gpio_get_level((gpio_num_t)I2S_BCLK_PIN);
    wHigh += gpio_get_level((gpio_num_t)I2S_WS_PIN);
    dHigh += gpio_get_level((gpio_num_t)I2S_DIN_PIN);
  }
  char clk[160];
  snprintf(clk, sizeof(clk), ",\"bclkHighPct\":%lu,\"wsHighPct\":%lu,\"dinHighPct\":%lu",
           (unsigned long)(bHigh * 100 / SAMPLES), (unsigned long)(wHigh * 100 / SAMPLES),
           (unsigned long)(dHigh * 100 / SAMPLES));

  char out[512];
  snprintf(out, sizeof(out),
           "{\"frames\":%lu,\"slot\":%d,"
           "\"L\":{\"min\":%ld,\"max\":%ld,\"avgAbs\":%lld,\"allOnes\":%lu,\"zeros\":%lu},"
           "\"R\":{\"min\":%ld,\"max\":%ld,\"avgAbs\":%lld,\"allOnes\":%lu,\"zeros\":%lu},"
           "\"raw\":\"%s\",\"din\":%d,\"dinLevel\":%d}",
           (unsigned long)n, s_micSlot,
           (long)mn[0], (long)mx[0], n ? sumAbs[0] / n : 0, (unsigned long)ones[0], (unsigned long)zeros[0],
           (long)mn[1], (long)mx[1], n ? sumAbs[1] / n : 0, (unsigned long)ones[1], (unsigned long)zeros[1],
           first.c_str(), I2S_DIN_PIN, gpio_get_level((gpio_num_t)I2S_DIN_PIN));
  String res(out);
  res.remove(res.length() - 1);   // drop closing brace
  res += clk;
  res += "}";
  return res;
}

void Audio::setVolume(uint8_t v) { s_volume = v > 100 ? 100 : v; g_state.volume = s_volume; }
uint8_t Audio::volume() { return s_volume; }

size_t Audio::record(int16_t* out, size_t maxSamples, std::function<bool()> stopRequested) {
  if (!MIC_ENABLED) return 0;
  xSemaphoreTake(s_rxLock, portMAX_DELAY);
  size_t br = 0;
  // Drop stale DMA data captured while idle
  for (int i = 0; i < 8; i++) i2s_read(PORT, s_rx, sizeof(s_rx), &br, 20);

  float px = 0, py = 0;                 // DC-blocking high-pass filter state
  size_t n = 0;
  bool speech = false;
  uint32_t t0 = millis(), lastVoice = t0;

  while (n < maxSamples) {
    if (stopRequested && stopRequested()) break;
    if (i2s_read(PORT, s_rx, sizeof(s_rx), &br, 200) != ESP_OK || br == 0) continue;
    int frames = br / 8;
    uint64_t sumSq = 0;
    for (int f = 0; f < frames && n < maxSamples; f++) {
      float x = (float)(s_rx[f * 2 + s_micSlot] >> MIC_GAIN_SHIFT);
      float y = x - px + 0.995f * py;
      px = x; py = y;
      int32_t v = (int32_t)y;
      if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
      out[n++] = (int16_t)v;
      sumSq += (int64_t)v * v;
    }
    uint32_t rms = (uint32_t)sqrt((double)sumSq / frames);
    g_state.speakLevel = min<uint32_t>(255, rms / 20);   // animates listening bars
    uint32_t now = millis();
    if (rms > VAD_START_RMS) { speech = true; lastVoice = now; }
    if (!speech && now - t0 > VAD_NO_SPEECH_MS) break;
    if (speech && now - lastVoice > VAD_SILENCE_MS) break;
  }
  g_state.speakLevel = 0;
  xSemaphoreGive(s_rxLock);
  return speech ? n : 0;
}

size_t Audio::listen(int16_t* out, size_t maxSamples, std::function<bool()> abort) {
  if (!MIC_ENABLED) return 0;
  if (xSemaphoreTake(s_rxLock, pdMS_TO_TICKS(50)) != pdTRUE) return 0;
  const int PRE = 20;                          // 20 x 16 ms = ~0.3 s kept from before speech
  static int16_t ring[PRE][CHUNK];
  int head = 0, filled = 0, loud = 0;
  float px = 0, py = 0;
  size_t br = 0, n = 0;

  auto readChunk = [&](int16_t* dst, uint32_t& rms) -> int {
    if (i2s_read(PORT, s_rx, sizeof(s_rx), &br, 100) != ESP_OK || br == 0) return 0;
    int frames = br / 8;
    uint64_t sq = 0;
    for (int f = 0; f < frames; f++) {
      float x = (float)(s_rx[f * 2 + s_micSlot] >> MIC_GAIN_SHIFT);
      float y = x - px + 0.995f * py;
      px = x; py = y;
      int32_t v = (int32_t)y;
      if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
      dst[f] = (int16_t)v;
      sq += (int64_t)v * v;
    }
    for (int f = frames; f < CHUNK; f++) dst[f] = 0;
    rms = (uint32_t)sqrt((double)sq / frames);
    return frames;
  };

  // 1) wait for speech, keeping a short pre-roll. The trigger level follows the room's own
  //    background noise (fan, AC...), so only sound clearly louder than the room starts a recording.
  static float noise = 300;                    // remembered between calls
  for (int i = 0; i < 6; i++) i2s_read(PORT, s_rx, sizeof(s_rx), &br, 20);   // drop stale audio
  while (true) {
    if (abort && abort()) { xSemaphoreGive(s_rxLock); return 0; }
    uint32_t rms = 0;
    if (!readChunk(ring[head], rms)) continue;
    head = (head + 1) % PRE;
    if (filled < PRE) filled++;
    float trigger = max((float)LISTEN_START_RMS, noise * 3.0f);
    if (rms > trigger) { if (++loud >= 3) break; }            // ~50 ms of real sound, not a click
    else { loud = 0; noise = noise * 0.98f + rms * 0.02f; }   // learn the background level
  }
  float keepLevel = max((float)LISTEN_START_RMS * 0.6f, noise * 2.0f);

  // 2) copy the pre-roll, then record until quiet
  for (int k = 0; k < filled && n + CHUNK <= maxSamples; k++) {
    memcpy(out + n, ring[(head - filled + k + PRE) % PRE], CHUNK * sizeof(int16_t));
    n += CHUNK;
  }
  uint32_t lastVoice = millis();
  while (n + CHUNK <= maxSamples) {
    if (abort && abort()) { n = 0; break; }
    uint32_t rms = 0;
    int got = readChunk(out + n, rms);
    if (!got) continue;
    n += got;
    if (rms > keepLevel) lastVoice = millis();
    if (millis() - lastVoice > LISTEN_SILENCE_MS) break;
  }
  xSemaphoreGive(s_rxLock);
  return n;
}

void Audio::playChirps(const Chirp* seq, size_t count) {
  TxGuard guard;
  const float SR = AUDIO_SAMPLE_RATE;
  float phase = 0;
  size_t bw;
  for (size_t c = 0; c < count; c++) {
    const Chirp& ch = seq[c];
    uint32_t total = (uint32_t)(SR * ch.ms / 1000);
    uint32_t attack = SR * 0.008f, release = SR * 0.030f;
    uint32_t n = 0;
    while (n < total) {
      int frames = min<uint32_t>(CHUNK, total - n);
      for (int f = 0; f < frames; f++, n++) {
        int32_t v = 0;
        if (ch.f0) {
          float tt = (float)n / total;
          float freq = ch.f0 + (ch.f1 - ch.f0) * tt;
          if (ch.vibrato) freq += ch.vibrato * sinf(2 * PI * 7.0f * n / SR);
          phase += 2 * PI * freq / SR;
          if (phase > 2 * PI) phase -= 2 * PI;
          float env = 1.0f;
          if (n < attack) env = (float)n / attack;
          else if (total - n < release) env = (float)(total - n) / release;
          // fundamental + a little 2nd harmonic = soft "robot voice" timbre
          float s = sinf(phase) * 0.8f + sinf(2 * phase) * 0.2f;
          v = (int32_t)(s * env * 14000);
        }
        int32_t slot = toSlot(v);
        s_io[f * 2] = slot; s_io[f * 2 + 1] = slot;
      }
      i2s_write(PORT, s_io, frames * 8, &bw, portMAX_DELAY);
    }
  }
  writeSilence(40);
}

void Audio::playPcm(const int16_t* pcm, size_t samples) {
  TxGuard guard;
  size_t i = 0, bw;
  while (i < samples) {
    int frames = min<size_t>(CHUNK, samples - i);
    for (int f = 0; f < frames; f++) {
      int32_t v = toSlot(pcm[i + f]);
      s_io[f * 2] = v; s_io[f * 2 + 1] = v;
    }
    i2s_write(PORT, s_io, frames * 8, &bw, portMAX_DELAY);
    i += frames;
  }
  writeSilence(60);
}

bool Audio::playWavStream(Stream* s, int contentLength) {
  TxGuard guard;
  s->setTimeout(1500);
  uint8_t hdr[12];
  if (!readExact(s, hdr, 12) || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
    Serial.println("[audio] not a WAV stream");
    return false;
  }
  uint16_t channels = 1, bits = 16;
  uint32_t rate = AUDIO_SAMPLE_RATE, dataLen = 0;
  while (true) {
    uint8_t ch[8];
    if (!readExact(s, ch, 8)) return false;
    uint32_t sz = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
    if (!memcmp(ch, "fmt ", 4)) {
      uint8_t fmt[40];
      uint32_t take = min<uint32_t>(sz, sizeof(fmt));
      if (!readExact(s, fmt, take)) return false;
      for (uint32_t k = take; k < sz; k++) s->read();
      channels = fmt[2] | (fmt[3] << 8);
      rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      bits = fmt[14] | (fmt[15] << 8);
    } else if (!memcmp(ch, "data", 4)) {
      dataLen = sz;
      break;
    } else {
      for (uint32_t k = 0; k < sz + (sz & 1); k++) s->read();
    }
  }
  if (bits != 16 || channels < 1 || channels > 2) {
    Serial.printf("[audio] unsupported wav: %u bit, %u ch\n", bits, channels);
    return false;
  }

  const size_t frameBytes = 2 * channels;
  static uint8_t inBuf[2048];
  uint32_t remaining = dataLen ? dataLen : 0xFFFFFFFF;
  float step = (float)rate / AUDIO_SAMPLE_RATE, phase = 0;
  int outN = 0;
  size_t bw;

  while (remaining > 0) {
    size_t want = min<uint32_t>(remaining, sizeof(inBuf));
    want -= want % frameBytes;
    int got = s->readBytes(inBuf, want);
    if (got <= 0) break;
    remaining -= got;
    int frames = got / frameBytes;
    int16_t* sm = (int16_t*)inBuf;
    uint32_t peak = 0;
    for (; phase < frames; phase += step) {
      int idx = (int)phase * channels;
      int32_t v = channels == 2 ? ((int32_t)sm[idx] + sm[idx + 1]) / 2 : sm[idx];
      uint32_t a = abs(v);
      if (a > peak) peak = a;
      int32_t slot = toSlot(v);
      s_io[outN * 2] = slot; s_io[outN * 2 + 1] = slot;
      if (++outN == CHUNK) {
        i2s_write(PORT, s_io, sizeof(s_io), &bw, portMAX_DELAY);
        outN = 0;
      }
    }
    phase -= frames;
    g_state.speakLevel = min<uint32_t>(255, peak / 64);
  }
  if (outN) i2s_write(PORT, s_io, outN * 8, &bw, portMAX_DELAY);
  writeSilence(80);
  g_state.speakLevel = 0;
  return true;
}
