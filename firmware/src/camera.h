#pragma once
#include <Arduino.h>
#include "esp_camera.h"

namespace Camera {
  bool begin();                 // must run BEFORE Audio::begin() (shared GPIO5)
  camera_fb_t* grab();          // returns nullptr on failure
  void release(camera_fb_t* fb);
}
