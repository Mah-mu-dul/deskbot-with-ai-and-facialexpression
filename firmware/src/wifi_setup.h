#pragma once
#include <Arduino.h>

// Wi-Fi setup without editing code.
//
// When the bot has no saved Wi-Fi, can't join it, or the button is held at
// power-on, it opens its own network "DeskBot-Setup". Joining it pops up a
// setup page (captive portal) to pick the Wi-Fi, enter the password and the
// PC address of the DeskBot server. Everything is saved in flash.
namespace WifiSetup {
  bool hasCredentials();
  // Join the saved network. Returns true when online.
  bool connectSaved(uint32_t timeoutMs);
  // Run the setup portal. Restarts the bot after saving, or after
  // `timeoutMs` (0 = wait forever) so it can retry the saved network.
  void runPortal(const char* reason, uint32_t timeoutMs);
  // Ask for the portal on the next boot (used by the bot's web page).
  void requestPortalOnNextBoot();
  bool portalRequested();      // and clears the request
  String savedSsid();
  String normaliseServer(const String& url);   // "192.168.0.5" -> "http://192.168.0.5:8000"
}
