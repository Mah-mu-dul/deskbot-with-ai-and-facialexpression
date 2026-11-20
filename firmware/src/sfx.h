#pragma once
#include "state.h"

// Emo-style mood sounds: a short synthesized "voice" phrase for each
// expression, played on the speaker (buzzer fallback if audio failed).
namespace Sfx {
  void play(Face f);
  // Call from loop(): plays a sound when the bot's expression changes.
  void update(bool audioOk);
}
