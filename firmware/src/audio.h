#pragma once
#include <Arduino.h>
#include <functional>

// Full-duplex I2S: INMP441 mic (RX) + MAX98357A amp (TX) on one bus.
namespace Audio {
  bool begin();

  // Record mono 16-bit PCM at AUDIO_SAMPLE_RATE until silence (VAD),
  // max samples reached, or stopRequested() returns true.
  // Returns number of samples written to `out` (0 = nobody spoke).
  size_t record(int16_t* out, size_t maxSamples, std::function<bool()> stopRequested);

  // Hands-free: wait for speech, then record it (with ~0.3 s from before it got loud)
  // until LISTEN_SILENCE_MS of quiet. Returns 0 if abort() turned true first.
  size_t listen(int16_t* out, size_t maxSamples, std::function<bool()> abort);

  // Play a WAV (16-bit PCM mono/stereo, any common rate is resampled
  // by nearest-neighbour to 16 kHz) from a network stream.
  bool playWavStream(Stream* s, int contentLength);

  // Play raw 16-bit mono samples
  void playPcm(const int16_t* pcm, size_t samples);

  // One synthesized robot "syllable": pitch glides f0 -> f1 over `ms`,
  // with optional vibrato (Hz of wobble depth). f0 == 0 means a short pause.
  struct Chirp { uint16_t f0, f1, ms; uint8_t vibrato; };
  void playChirps(const Chirp* seq, size_t count);

  // Diagnostics: read ~0.5 s of raw I2S data and describe both slots as JSON.
  String micDebugJson();
  // Re-init I2S as RX-only and/or shut the camera down, then measure (debug only).
  String micTest(bool rxOnly, bool noCam);

  void setVolume(uint8_t vol0to100);
  uint8_t volume();
}
