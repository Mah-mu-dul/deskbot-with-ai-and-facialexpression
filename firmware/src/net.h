#pragma once
#include <Arduino.h>

// Command sent by the brain server (or the local web page) to the bot.
struct BotCmd {
  char type[10];    // say | face | mood | beep | focus | vol | caption
  char v[128];      // main value (url path, face name, pattern, action...)
  char text[192];   // optional caption text
  int32_t a;        // optional numbers (duration ms, work min...)
  int32_t b;
};

namespace Net {
  void begin();                         // load settings, start upload task
  String serverUrl();
  void setServerUrl(const String& url);
  bool pushCmd(const BotCmd& c);        // enqueue a command for loop()
  bool popCmd(BotCmd& c);               // non-blocking

  // Voice round-trip: upload recorded PCM, play the spoken answer.
  // Blocks until playback finishes. Returns false on network error.
  // `buf` = 44 spare bytes (WAV header is written there) + int16 PCM samples.
  // autoMode = hands-free: the server stays quiet (204) unless the bot was called by name.
  bool voiceRoundTrip(uint8_t* buf, size_t samples, bool autoMode = false);

  // Download and play a WAV from the server (path like "/api/tts/abc.wav")
  bool playFromServer(const char* path);
}
