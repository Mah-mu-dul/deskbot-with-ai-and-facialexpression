#pragma once
#include <Arduino.h>

// ---- Faces the TFT can show ------------------------------------------
enum class Face : uint8_t {
  Neutral, Happy, Sad, Angry, Surprised, Sleepy, Love,
  Focused, Listening, Thinking, Speaking, Warning, Confused,
  COUNT
};

// What the bot is doing right now (takes priority over mood faces)
enum class Activity : uint8_t { Idle, Listening, Thinking, Speaking };

enum class FocusPhase : uint8_t { Off, Work, Break, Paused };

const char* faceName(Face f);
Face faceFromName(const char* name, Face fallback = Face::Neutral);
const char* focusPhaseName(FocusPhase p);

// ---- Shared state (written by loop/net task, read by display task) ---
struct BotState {
  volatile Face     mood = Face::Neutral;        // from emotion tracking (server)
  volatile Face     overrideFace = Face::Neutral; // temporary expression
  volatile uint32_t overrideUntil = 0;
  volatile Activity activity = Activity::Idle;
  volatile uint8_t  speakLevel = 0;              // 0..255 mouth opening while talking

  volatile float    temperature = NAN;
  volatile float    humidity = NAN;

  volatile bool     wifiOk = false;
  volatile bool     serverOk = false;
  volatile bool     cameraOk = false;
  volatile bool     sensorOk = false;

  volatile FocusPhase focusPhase = FocusPhase::Off;
  volatile uint32_t focusRemainingSec = 0;
  volatile uint32_t focusTotalSec = 0;
  volatile uint16_t focusSessions = 0;

  volatile uint8_t  volume = 70;
  volatile uint8_t  screenMode = 0;              // 0 face, 1 room, 2 info (double-click cycles)
  volatile uint32_t lastServerOkMs = 0;          // millis() of the last good server reply
  volatile int8_t   resetCountdown = -1;         // seconds left while holding for reset, -1 = none
  volatile bool     sfx = true;                  // mood sounds on/off (from server settings)
  volatile bool     autoListen = true;           // hands-free listening (from server settings)
  volatile bool     audioBusy = false;           // speaker is playing (don't listen to ourselves)
  volatile uint32_t audioQuietUntil = 0;         // millis() until the room echo has died down
  char              aiState[12] = "unknown";     // ok | no_key | offline | busy | error | unknown
};

extern BotState g_state;

// Caption text shown at the bottom of the screen (thread-safe helpers)
void setCaption(const String& text, uint32_t durationMs = 5000);
bool getCaption(char* out, size_t outLen);   // false if no active caption

// Pending events reported to the server with the next frame ("work_done,focus_start")
void pushEvent(const char* ev);
String takeEvents();

Face currentFace();     // resolves activity > override > mood
Face expressionFace();  // override > mood (ignores activity) - eyes while speaking
void showFace(Face f, uint32_t durationMs);
