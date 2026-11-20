# DeskBot 🤖

An intelligent, emotion-aware desk companion and study-buddy robot inspired by Emo. Built on an **ESP32-S3 CAM** paired with a 3.5" SPI color display, DeskBot monitors your facial expressions, tracks Pomodoro study sessions, alerts you when you get distracted by your smartphone, and interacts hands-free in both **English and Bangla (বাংলা)**.

---

## 📌 Table of Contents
- [System Architecture](#-system-architecture)
- [Key Features](#-key-features)
- [Hardware & Bill of Materials](#-hardware--bill-of-materials)
- [Wiring & Pinouts](#-wiring--pinouts)
- [Getting Started](#-getting-started)
  - [1. Flash Firmware](#1-flash-the-firmware)
  - [2. Run the Brain Server](#2-run-the-brain-server)
  - [3. Connect Wi-Fi (Captive Portal)](#3-connect-wi-fi-via-captive-portal)
  - [4. Add AI API Key](#4-configure-ai-brain)
- [Hardware Controls & Screens](#-hardware-controls--screens)
- [Multimodal AI & Speech Engine](#-multimodal-ai--speech-engine)
- [Web Dashboard](#-web-dashboard)
- [Standalone App Packaging](#-standalone-app-packaging)
- [Troubleshooting](#-troubleshooting)

---

## 🏗 System Architecture

DeskBot splits responsibilities between low-latency hardware control on the microcontroller and heavy machine intelligence on the host PC:

```
 ESP32-S3 CAM (firmware/)                         Host PC / Server (server/)
 ─────────────────────────                        ─────────────────────────────────────────
 Camera JPEG frames (~3 fps)    ───────────────▶  Face detection & emotion (OpenCV + FER+)
 Sensor telemetry (AHT10)                         Phone distraction detector (YOLOv8n)
 Mic audio (16 kHz mono WAV)                      Speech-to-Text (faster-whisper / Gemini)
                                                  Conversational Brain (Gemini / Claude)
 Procedural animated eyes       ◀───────────────  Text-to-Speech (Edge-TTS / Windows SAPI5)
 I2S audio stream / mouth                         Web Dashboard (http://<pc-ip>:8000)
 Buzzer chimes & timer events                     Control commands: [say, face, beep, focus]
```

- **ESP32-S3 CAM**: Manages SPI display rendering (12+ FPS), I2S audio output, microphone capture, AHT10 sensor reading, tactile button inputs, and camera streaming.
- **Python Brain Server**: Fast asynchronous FastAPI server executing facial emotion analysis, YOLOv8 object detection, speech processing, LLM reasoning, and persistent session statistics.

---

## ✨ Key Features

- **Procedural Animated Expressions**: 12 distinct facial states (neutral, happy, sad, angry, surprised, sleepy, love, focused, listening, thinking, speaking with lip-sync, and warning).
- **Audio Feedback & Mood Chirps**: Chimes and synthesized robot chirps accompanying emotional transitions and timer alerts.
- **Visual Emotion Recognition**: Real-time facial expression tracking via FER+ neural network.
- **Study Focus Mode**: Built-in Pomodoro workflow (work and break cycles) triggered via button, voice, or dashboard.
- **Phone Distraction Detection**: Continuous YOLOv8 vision monitoring during focus time; beeps and reminds you out loud when you check your phone.
- **Room Climate Telemetry**: Live AHT10 temperature and humidity monitoring with proactive health and comfort tips.
- **Hands-Free Voice Assistant**: Continuous voice activation via wake words ("Hey Desky", "Desky", or Bangla "অলি").
- **Bilingual Conversations**: Native support for English and Bengali speech recognition, natural conversation, and voice synthesis.
- **Offline Resilient**: Clock, date, room temperature, focus timer, and basic dialogue work completely offline without internet or API keys.
- **Session Reports**: Historical analytics for daily focus hours, distraction counts, mood trends, and focus streaks.

---

## 🛠 Hardware & Bill of Materials

| Component | Specification | Purpose |
|---|---|---|
| **Microcontroller** | ESP32-S3-WROOM-1 CAM (N16R8 or N8R8) | Core controller (**Octal PSRAM required**) |
| **Camera Sensor** | OV2640 / OV3660 ribbon module | Face and distraction tracking |
| **Display Panel** | 3.5" SPI TFT 480x320 (ILI9488 or ST7796) | Facial animation and system screens |
| **Audio Amplifier** | MAX98357A I2S Mono Class-D Amplifier | Speaker output for voice and chirps |
| **Speaker** | 4 Ω to 8 Ω, 3 Watt | Audio reproduction |
| **Microphone** | INMP441 Omnidirectional Digital I2S Mic | Hands-free listening and voice queries |
| **Climate Sensor** | AHT10 Digital Temperature & Humidity (I2C) | Environmental tracking |
| **Buzzer** | 2-pin passive buzzer | System notifications and alert tones |
| **Push Button** | Tactile momentary push button | Mode switching and timer control |
| **Host PC** | Windows 10/11 on same local network | Runs Python brain server or `.exe` |

---

## 🔌 Wiring & Pinouts

All logic signals operate at **3.3 V**. Connect MAX98357A power to **5 V** for higher volume output.

### Pin Connection Reference

| Module | Pin | ESP32-S3 GPIO | Details |
|---|---|---|---|
| **INMP441 Mic** | SD | **GPIO 5** | Shared with camera SCCB clock; released to mic after camera init |
| | SCK | **GPIO 1** | Shared with amplifier BCLK (duplex I2S bus) |
| | WS | **GPIO 2** | Shared with amplifier LRC/WS |
| | L/R | GND | Left channel mode |
| | VDD / GND | 3V3 / GND | Power supply |
| **MAX98357A Amp**| DIN | **GPIO 3** | I2S Data Transmission |
| | BCLK | **GPIO 1** | Clock line (shared with mic SCK) |
| | LRC | **GPIO 2** | Word select (shared with mic WS) |
| | VIN / GND | 5V / GND | Power supply |
| **AHT10 Sensor** | SDA | **GPIO 42** | I2C Data |
| | SCL | **GPIO 0** | I2C Clock (strapping pin pulled high by module) |
| | VIN / GND | 3V3 / GND | Power supply |
| **SPI TFT** | MOSI | **GPIO 47** | SPI Master Out |
| | SCLK | **GPIO 48** | SPI Clock |
| | CS | **GPIO 39** | Chip Select |
| | DC | **GPIO 40** | Data / Command Select |
| | RST | **GPIO 41** | Display Reset |
| | BL | 3V3 | Backlight power |
| | VCC / GND | 3V3 / GND | Power supply |
| **Buzzer** | + | **GPIO 38** | PWM via LEDC channel 4 |
| | - | GND | Ground |
| **Push Button** | Leg 1 | **GPIO 45** | Internal pull-up enabled (active LOW) |
| | Leg 2 | GND | Ground |

> [!WARNING]
> **Do not connect INMP441 SD to GPIO 15.** GPIO 15 is reserved for the 20 MHz camera master clock (XCLK). Wiring to GPIO 15 can damage the microphone or microcontroller.

> [!NOTE]
> GPIO 35, 36, and 37 are internally occupied by Octal PSRAM on N16R8/N8R8 boards. Keep them free.

---

## 🚀 Getting Started

### 1. Flash the Firmware
1. Open the `firmware/` directory inside **VS Code** with the **PlatformIO** extension installed.
2. Select your display controller in `firmware/include/config.h` (default: `DISPLAY_ILI9488_480x320`).
3. If using an 8 MB board (N8R8), update `board_upload.flash_size = 8MB` in `platformio.ini`.
4. Connect the board via the USB-UART COM port.
5. Click **PlatformIO: Upload** or run:
   ```bash
   pio run -t upload
   ```

### 2. Run the Brain Server

#### Option A: Standalone Binary (Recommended for non-devs)
Download or build `DeskBot.exe` (see [Standalone Packaging](#-standalone-app-packaging)), then double-click the executable.

#### Option B: From Source (Python 3.10 – 3.12)
1. Open a terminal in `server/`:
   ```powershell
   python -m venv .venv
   .\.venv\Scripts\activate
   pip install --upgrade pip
   pip install -r requirements.txt
   ```
2. Download required model files:
   ```powershell
   python download_models.py
   ```
3. Set up environment:
   ```powershell
   copy .env.example .env
   ```
4. Start the server:
   ```powershell
   python app.py
   ```

### 3. Connect Wi-Fi via Captive Portal
1. On initial startup, the bot creates a Wi-Fi hotspot named **DeskBot-Setup** (password: `11221122`).
2. Join the network from your phone or laptop.
3. In the setup portal (`http://192.168.4.1`), select your 2.4 GHz network, enter its password, and input your PC local IP.
4. Hit **Save and connect**. The bot restarts and connects automatically.

### 4. Configure AI Brain
1. Navigate to `http://localhost:8000` (or your host IP) in any web browser.
2. Under **Settings → AI brain**, select **Gemini**, input your Google AI Studio API key, and save.
3. Test connectivity with the **Test connection** button.

---

## 🎮 Hardware Controls & Screens

### Button Functionality

| Gesture | State | Result |
|---|---|---|
| **Single Click** | Normal | Triggers voice listening prompt |
| **Single Click** | Focus Active | Pauses or resumes focus session |
| **Double Click** | Any | Cycles screen: **Face → Climate → Info** |
| **Long Press (1s)**| Idle | Starts Pomodoro focus mode |
| **Long Press (1s)**| Focus Active | Stops focus mode |
| **5s Hold** | Info Screen | Initiates Factory Reset (reboots into setup AP) |
| **Hold during Boot**| Power-on | Forces setup portal mode |

### Display Screens

- **Face Screen**: Procedural eye animations, dynamic mouth movement during speech, temperature, and connectivity dots.
- **Room Screen**: Large readout of temperature and humidity along with automated comfort tips.
- **Info Screen**: IP address, Wi-Fi SSID, RSSI, server status, memory stats, and firmware build version.

---

## 🧠 Multimodal AI & Speech Engine

- **Voice Input**: Evaluates audio locally using `faster-whisper` for English, routing conversational turns to Gemini or Claude.
- **Bangla Multimodal Speech**: Streams raw audio to Gemini to produce both Bengali transcriptions and culturally accurate spoken responses.
- **Vision Pipeline**: Combines Haar cascades for face finding, FER+ ONNX for emotion classification, and YOLOv8n for cell phone distraction detection.
- **Offline Fallback Matrix**:

| Capability | Online Mode | Offline Mode |
|---|---|---|
| **English Speech-to-Text** | Local faster-whisper | Local faster-whisper |
| **Bangla Speech-to-Text** | Gemini Multimodal | Not Available |
| **Voice Output (TTS)** | Microsoft Edge Neural | Windows SAPI5 (pyttsx3) |
| **Focus & Climate Queries**| Immediate Local Match | Immediate Local Match |
| **General Knowledge** | Gemini / Claude LLM | Built-in offline dialogue |

---

## 🖥 Web Dashboard

Accessible from any device on your Wi-Fi at `http://<host-ip>:8000`:

- **Home**: Live animated eye preview, real-time annotated camera feed, and environmental gauges.
- **Chat**: Web conversational terminal with text-to-speech toggles and bilingual input.
- **Focus**: Visual timer wheel with custom work/break sliders.
- **Bot**: Manual expression trigger, audio tester, and event log.
- **Reports**: Daily and weekly productivity metrics, hourly distribution graphs, and distraction logs.
- **Settings**: AI keys, custom persona, wake word rules, and camera sensitivity.

---

## 📦 Standalone App Packaging

To package the entire server and neural models into a standalone Windows binary:

1. Ensure `server/models/` contains `emotion-ferplus-8.onnx`, `whisper-base/`, and `yolov8n.onnx`.
2. Convert YOLOv8 to ONNX if needed:
   ```powershell
   python -c "from ultralytics import YOLO; YOLO('yolov8n.pt').export(format='onnx', imgsz=640, opset=12)"
   move yolov8n.onnx server/models/
   ```
3. Run the packaging script:
   ```cmd
   cd server
   build_standalone.bat
   ```
4. Output executable is placed inside `dist/DeskBot/DeskBot.exe`.

---

## ❓ Troubleshooting

| Issue | Cause | Fix |
|---|---|---|
| **Bot ignores voice calls** | Microphone disabled or noisy room | Verify `MIC_ENABLED 1` in `config.h`. Check Info screen for `ears on`. Ensure queries begin with wake phrase. Test stream at `http://<bot-ip>/api/mic`. |
| **Server dot is RED** | Host server unreachable | Verify PC and bot share the same 2.4 GHz subnet. Allow port 8000 in Windows Firewall. Verify IP via Info screen. |
| **Screen is all white** | Wrong display driver | Check driver selection in `firmware/include/config.h` (e.g. swap ILI9488 and ST7796). |
| **Display image shifted/scrambled** | SPI clock too high | Reduce `TFT_SPI_HZ` in `config.h` to `16000000` or `10000000`. |
| **No phone alerts in study mode** | Low confidence or inactive session | Ensure focus session is running. Verify detection entries in **Bot → Recent activity**. Lower confidence threshold in Settings. |
| **Upload sync failure** | Board not in bootloader | Hold **BOOT** button, press **RST**, release **BOOT**, and restart upload. |

---

## 📜 License

This project is open-source under the MIT License.
