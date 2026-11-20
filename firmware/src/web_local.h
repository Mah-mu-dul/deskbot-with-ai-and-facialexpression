#pragma once
// Small web page + REST API served by the bot itself (http://deskbot.local).
// The full dashboard runs on the brain server; this page works even when
// the server is offline (status, camera snapshot, timer, faces, server URL).
namespace WebLocal {
  void begin();
  void handle();   // call from loop()
}
