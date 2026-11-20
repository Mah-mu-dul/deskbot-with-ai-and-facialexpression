#pragma once
// =====================================================================
//  DeskBot - hardware & behaviour configuration
//  Board: ESP32-S3 CAM (ESP32-S3-WROOM-1 N16R8, OV2640/OV3660 camera)
// =====================================================================

// ---------------- I2S audio (one full-duplex bus) --------------------
// The INMP441 mic and MAX98357A amp share BCLK + WS on purpose:
// a single I2S peripheral clocks both (TX -> amp, RX <- mic).
#define I2S_BCLK_PIN      1   // INMP441 SCK  + MAX98357A BCLK
#define I2S_WS_PIN        2   // INMP441 WS   + MAX98357A LRC
#define I2S_DOUT_PIN      3   // MAX98357A DIN
// Microphone on/off. 0 = speaker only; talk to the bot from the dashboard chat.
#define MIC_ENABLED       1
#define I2S_DIN_PIN       5   // INMP441 SD (camera SCCB clock pin; released after camera setup)
// !! Never use GPIO15 for the mic: it is the camera clock (XCLK) output.
// !! GPIO5 is the camera SCCB clock and has a pull-up on the camera board.

#define AUDIO_SAMPLE_RATE 16000
#define MIC_GAIN_SHIFT    15      // 32-bit mic word >> shift -> int16 (lower = louder; 12 clipped)
#define DEFAULT_VOLUME    70      // 0..100
#define RECORD_MAX_SEC    10      // max utterance length
#define VAD_START_RMS     450     // RMS that counts as "speech started"
#define VAD_SILENCE_MS    800     // button: stop after this much silence after speech
// Hands-free listening (on/off from the dashboard). Speech louder than
// LISTEN_START_RMS starts a recording; LISTEN_SILENCE_MS of quiet ends it.
#define LISTEN_START_RMS  600     // minimum; the real level adapts to room noise
#define LISTEN_SILENCE_MS 700
#define LISTEN_MAX_SEC    6
#define VAD_NO_SPEECH_MS  5000    // give up if nobody speaks

// ---------------- AHT10 (I2C) -----------------------------------------
#define AHT_SDA_PIN       42
#define AHT_SCL_PIN       0       // strapping pin: fine, module pull-ups keep it HIGH at boot
#define SENSOR_PERIOD_MS  3000

// ---------------- Buzzer / button -------------------------------------
#define BUZZER_PIN        38
#define BUZZER_LEDC_CH    4       // LEDC ch 0/1 (timer 0) are used by camera XCLK
#define BUTTON_PIN        45      // to GND, internal pull-up

// ---------------- TFT (SPI) -------------------------------------------
#define TFT_MOSI          47
#define TFT_SCLK          48
#define TFT_CS            39
#define TFT_DC            40
#define TFT_RST           41
// ILI9488 takes 3 bytes per pixel; if a byte is lost the picture slides sideways and the
// colours rotate. 20 MHz is safe with longer wires / battery power. (ESP32 SPI: 80/n MHz)
#define TFT_SPI_HZ        20000000

// Blank margin (pixels) kept clear because the enclosure covers the screen edges.
// Increase if anything is still hidden behind the frame.
#define SCREEN_PAD_X      34
#define SCREEN_PAD_Y      30
#define SCREEN_FRAME_R    22      // rounded frame drawn around the content
#define FW_VERSION        "1.6"
#define RESET_HOLD_MS     5000    // hold the button this long on the Info screen to reset everything

// Pick exactly ONE display driver:
#define DISPLAY_ILI9488_480x320     // 3.5" SPI 480x320 "v1.0" module (most common)
// #define DISPLAY_ST7796_480x320   // 3.5"/4" SPI 480x320 with ST7796S chip
// #define DISPLAY_ST7789_240x240
// #define DISPLAY_ST7789_240x320
// #define DISPLAY_ST7789_135x240
// #define DISPLAY_ILI9341_240x320
// #define DISPLAY_ST7735_128x160
#define TFT_ROTATION      1       // 1/3 = landscape, 0/2 = portrait
#define FACE_FPS          12      // a 480x320 SPI panel can't refresh much faster

// ---------------- Camera (ESP32-S3 CAM / ESP32-S3-EYE pinout) ---------
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK     15
#define CAM_PIN_SIOD      4
#define CAM_PIN_SIOC      5
#define CAM_PIN_D7       16
#define CAM_PIN_D6       17
#define CAM_PIN_D5       18
#define CAM_PIN_D4       12
#define CAM_PIN_D3       10
#define CAM_PIN_D2        8
#define CAM_PIN_D1        9
#define CAM_PIN_D0       11
#define CAM_PIN_VSYNC     6
#define CAM_PIN_HREF      7
#define CAM_PIN_PCLK     13
#define CAM_FRAMESIZE    FRAMESIZE_VGA   // 640x480 - good for phone detection
#define CAM_JPEG_QUALITY 13              // 10 (best) .. 63
#define CAM_VFLIP        0
#define CAM_HMIRROR      0

// ---------------- Focus timer defaults ---------------------------------
#define FOCUS_WORK_MIN    25
#define FOCUS_BREAK_MIN    5

// ---------------- Networking -------------------------------------------
#define FRAME_INTERVAL_MS 300    // ~3 frames a second to the brain server (paused while the bot talks)
#define HTTP_TIMEOUT_MS   8000
#define MDNS_NAME         "deskbot"   // http://deskbot.local

// Wi-Fi setup portal (the bot's own network when it needs Wi-Fi details)
#define SETUP_AP_NAME     "DeskBot-Setup"
#define SETUP_AP_PASSWORD "11221122"  // 8+ characters (WPA2)
