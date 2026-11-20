# DeskBot

An intelligent, emotion-aware desk companion and study-buddy robot inspired by Emo. Powered by an **ESP32-S3 CAM** with a 3.5" SPI color display, DeskBot reads your facial expressions, tracks focus sessions, alerts you when you get distracted by your phone, and talks with you hands-free in **English and Bangla (বাংলা)**.

```
 ESP32-S3 CAM (firmware/)                         PC on the same Wi-Fi (server/)
 ─────────────────────────                        ──────────────────────────────────────
 Camera JPEG frames (~3 fps)    ───────────────▶  Face detection & emotion (OpenCV + FER+)
 Sensor telemetry (AHT10)                         Phone & distraction detection (YOLOv8n)
 Mic audio stream (16 kHz WAV)                    Speech-to-text (faster-whisper / Gemini)
                                                  AI Brain (Google Gemini or Anthropic Claude)
 Animated eyes & speaking mouth ◀───────────────  Text-to-speech (Edge-TTS / Windows SAPI5)
 Buzzer chimes, I2S speaker,                      Web Dashboard at http://<pc-ip>:8000
 Button, Focus timer state                        Commands: [say, face, beep, focus, vol]
```

The ESP32-S3 handles real-time hardware control, display rendering, audio playback, sensor polling, and camera streaming. The DeskBot Python server running on a PC on your local Wi-Fi handles the heavy AI workloads (computer vision, STT, LLM reasoning, and TTS).

---

## Key Features

- **Expressive Animated Eyes**: Smooth 12+ FPS rendering on a 3.5" color TFT (neutral, happy, sad, angry, surprised, sleepy, love, focused, listening, thinking, speaking with dynamic mouth opening, confused, and warning).
- **Mood Sounds & Audio Feedback**: Procedural synth chirps on state changes, buzzer chimes for timers and alerts, and clear I2S voice output.
- **Emotion Recognition**: Tracks user facial emotion via camera (FER+ ONNX model). The bot mirrors or responds empathetically to your mood.
- **Focus & Pomodoro Timer**: Dedicated work and break sessions initiated by voice, button, or web dashboard, with custom session durations.
- **Phone Distraction Detection**: Scans focus sessions using YOLOv8n. If you pick up your phone, the bot beeps, shows a warning face, and verbally nudges you back on task.
- **Room Climate Monitoring**: AHT10 sensor displays live temperature and relative humidity, providing comfort advice when too hot, dry, or humid.
- **Hands-Free Voice Assistant**: Continuous listening with adaptable VAD; wakes on "Hey Desky" (or custom name / Bangla "অলি"), answers out loud.
- **Bilingual English & Bangla**: Full conversational AI, built-in system lines, voice recognition, and TTS voices in both English and Bangla.
- **Rich Productivity Reports**: Logs every focus session, tracking focus minutes, completed sessions, distraction counts, mood trends, and streaks.
- **Resilient Offline Architecture**: Commands (time, date, room climate, focus controls, volume) and small talk work 100% offline without cloud AI or internet.
- **Modern Web Dashboard**: Real-time eye mirror, annotated camera stream, focus timer ring, chat console, hardware controls, and bilingual UI.

---

## Hardware Bill of Materials (BOM)

| Component | Specification | Purpose |
|---|---|---|
| **ESP32-S3 CAM Board** | ESP32-S3-WROOM-1 (N16R8 or N8R8) with OV2640 / OV3660 camera | Main microcontroller (**octal PSRAM required**) |
| **3.5" SPI TFT Display** | 480x320 resolution (ILI9488 driver standard) | Animated face, room climate, and system info |
| **I2S Audio Amplifier** | MAX98357A Mono Class-D Amp | Speaker driver for voice, sound effects, and chirps |
| **Speaker** | 4 Ω to 8 Ω (3 W recommended) | Audio output |
| **I2S Microphone** | INMP441 Omnidirectional Digital Mic | Voice input and hands-free listening |
| **Temperature & Humidity Sensor** | AHT10 (I2C interface) | Ambient room monitoring |
| **Buzzer** | 2-pin passive buzzer (or active) | Sound effects, button feedback, and timer alarms |
| **Push Button** | 2-pin momentary tactile button | Hardware controls (focus, voice, screens, reset) |
| **Host PC / Laptop** | Windows 10/11 on the same local Wi-Fi | Runs DeskBot brain server or standalone `.exe` |
| **Google Gemini API Key** | Free tier from Google AI Studio | Powers natural conversation and Bangla voice |

---

## Hardware Wiring

All logic levels are **3.3 V**. Power the MAX98357A amplifier from **5 V** for maximum volume. For complete wiring details, consult [docs/WIRING.md](docs/WIRING.md).

### Pin Mapping Table

| Module | Module Pin | ESP32-S3 GPIO | Notes |
|---|---|---|---|
| **INMP441 Mic** | SD | **GPIO 5** | Shared with camera SCCB clock line (released to mic after camera init). |
| | SCK | **GPIO 1** | Shared with amplifier BCLK (single full-duplex I2S bus). |
| | WS | **GPIO 2** | Shared with amplifier LRC/WS. |
| | L/R | GND | Selects left channel. |
| | VDD / GND | 3V3 / GND | |
| **MAX98357A Amp** | DIN | **GPIO 3** | I2S Data Out. |
| | BCLK | **GPIO 1** | Shared with mic SCK. |
| | LRC | **GPIO 2** | Shared with mic WS. |
| | GAIN / SD | Unconnected | Default 9 dB gain, stereo mixed down to mono. |
| | VIN / GND | 5V / GND | Connect speaker +/- to amplifier outputs. |
| **AHT10 Sensor** | SDA | **GPIO 42** | I2C Data. |
| | SCL | **GPIO 0** | I2C Clock (strapping pin; module pull-up keeps HIGH at boot). |
| | VIN / GND | 3V3 / GND | |
| **SPI TFT Display** | MOSI / SDA | **GPIO 47** | SPI Master Out Slave In. |
| | SCLK / SCL | **GPIO 48** | SPI Clock (on-board RGB LED may flicker; this is normal). |
| | CS | **GPIO 39** | Chip Select. |
| | DC | **GPIO 40** | Data / Command select. |
| | RST | **GPIO 41** | Display Reset. |
| | BL / LED | 3V3 | Backlight (always on). |
| | VCC / GND | 3V3 / GND | |
| **Buzzer** | + | **GPIO 38** | Driven by LEDC channel 4 (camera uses channels 0/1). |
| | - | GND | |
| **Push Button** | Leg 1 | **GPIO 45** | Internal pull-up enabled; active LOW. |
| | Leg 2 | GND | |
| **Camera** | Ribbon cable | Standard S3-CAM | XCLK 15, SIOD 4, SIOC 5, D0-D7 11/9/8/10/12/18/17/16, VSYNC 6, HREF 7, PCLK 13. |

> [!WARNING]
> **Never connect the microphone SD pin to GPIO 15.** GPIO 15 outputs the 20 MHz camera master clock (XCLK). Connecting mic SD to GPIO 15 will cause a hardware bus conflict and can damage the microphone or ESP32 pin.

> [!NOTE]
> **Octal PSRAM Notice**: GPIO 35, 36, and 37 are internally dedicated to the high-speed Octal PSRAM (OPI) on N16R8/N8R8 modules. Never wire external components to these pins.

---

## Step-by-Step Setup Guide

### Step 1: Wire the Hardware
Assemble the components on a breadboard or custom PCB according to the pin table above and [docs/WIRING.md](docs/WIRING.md).

### Step 2: Build and Flash the Firmware
1. Install **VS Code** and the **PlatformIO IDE** extension.
2. Open the `firmware/` directory in VS Code.
3. Verify settings in `firmware/include/config.h`:
   - Display driver: `DISPLAY_ILI9488_480x320` is selected by default. Uncomment alternate lines if using ST7796, ST7789, ILI9341, or ST7735.
   - Screen padding: `SCREEN_PAD_X` (34 px) and `SCREEN_PAD_Y` (30 px) create bezel margins for 3D printed enclosures.
4. Check `firmware/platformio.ini`:
   - If your board is an **N8R8** (8 MB flash), change `board_upload.flash_size = 16MB` to `8MB`.
5. Connect your ESP32-S3 CAM board via the **UART/COM** USB port (CH340/CH343).
6. Click **PlatformIO: Upload** (`pio run -t upload`) in the status bar.
   *(If the flasher fails to sync, hold BOOT, tap RST, release BOOT, and retry).*

### Step 3: Run the Server on Your PC

Choose one of two options:

#### Option A: Pre-built Standalone App (No Python required)
1. Build or download the standalone bundle (see [Building the Standalone App](#building-the-standalone-app)).
2. Double-click `DeskBot.exe`.
3. In the Windows Firewall prompt, click **Allow access** for Private networks so the ESP32 can connect.
4. The server console will display your local IP address:
   ```text
   DeskBot server
   Dashboard:        http://localhost:8000
   Bot server URL:   http://192.168.0.111:8000
   ```

#### Option B: Run from Source (Python 3.10 – 3.12)
1. Open PowerShell / Command Prompt in the `server/` directory.
2. Create and activate a virtual environment:
   ```powershell
   python -m venv .venv
   .\.venv\Scripts\Activate.ps1
   ```
3. Install dependencies:
   ```powershell
   pip install --upgrade pip
   pip install -r requirements.txt
   ```
4. Download the offline AI models:
   ```powershell
   python download_models.py
   ```
5. Copy configuration:
   ```powershell
   copy .env.example .env
   ```
6. Start the server:
   ```powershell
   python app.py
   ```

### Step 4: Wi-Fi & Brain Connection via Captive Portal
You do **not** need to hardcode Wi-Fi credentials into firmware.

1. Power on the bot. On first boot, the TFT shows **Wi-Fi setup** and launches an Access Point named **DeskBot-Setup** (password: `11221122`).
2. On your smartphone or laptop, connect to Wi-Fi **DeskBot-Setup**.
3. The portal opens automatically (or browse to `http://192.168.4.1`).
4. Select your 2.4 GHz home Wi-Fi network, enter its password, and enter your PC IP address (e.g., `192.168.0.111` or `http://192.168.0.111:8000`).
5. Click **Save and connect**. The bot restarts and joins your Wi-Fi.
6. The screen shows the face, and the three top-right status indicators turn green:
   - Left dot: **Wi-Fi** connected.
   - Middle dot: **Brain Server** reachable.
   - Right dot: **Camera** initialized.

### Step 5: Configure AI Brain
1. Open the dashboard at `http://localhost:8000` (or `http://<pc-ip>:8000`).
2. Obtain a free Gemini API key from [Google AI Studio](https://aistudio.google.com/apikey).
3. Navigate to **Settings → AI brain**.
4. Select **Gemini**, paste your API key, and click **Save settings**.
5. Click **Test connection** to confirm the model replies.
*(Optional: Choose Anthropic Claude and provide an `ANTHROPIC_API_KEY`).*

---

## Operating DeskBot

### Hardware Button Actions

| Action | Context | Result |
|---|---|---|
| **Click** | Idle | Activates voice listening (beep chime, speaks on release). |
| **Click** | Focus Active | Pauses or resumes the running focus session. |
| **Double-Click** | Anywhere | Cycles screen mode: **Face → Room Climate → Info Screen**. |
| **Long Press (~1s)** | Idle | Starts a new focus session (default 25 min work). |
| **Long Press (~1s)** | Focus Active | Stops the active focus session. |
| **Hold 5s** | On Info Screen | Initiates **Factory Reset** countdown (clears Wi-Fi & reboots to setup AP). |
| **Hold at Boot** | Power-on | Forces bot to enter **DeskBot-Setup** Wi-Fi captive portal. |

### Display Screens

```
┌─────────────────────────────────┐   ┌─────────────────────────────────┐   ┌─────────────────────────────────┐
│ 24.5°C  55%            ● ● ●   │   │                                 │   │ DeskBot v1.6        ● ● ●   │
│                                 │   │          ROOM CLIMATE           │   │                                 │
│          ╭──╮     ╭──╮          │   │                                 │   │ Server: http://192.168.0.111:8000│
│          │  │     │  │          │   │         24.5 °C                 │   │ Status: Connected               │
│          ╰──╯     ╰──╯          │   │         55 % Humidity           │   │ Wi-Fi:  Home-WiFi (-58 dBm)     │
│                                 │   │                                 │   │ IP:     192.168.0.45            │
│  [====================        ] │   │   Comfortable temperature.      │   │ Camera: OK  | Sensor: OK        │
│  Focus 18:24  Phone away!       │   │   Stay hydrated and focused!    │   │ Hold 5s to Factory Reset        │
└─────────────────────────────────┘   └─────────────────────────────────┘   └─────────────────────────────────┘
           1. Face Screen                       2. Room Screen                       3. Info Screen
```

1. **Face Screen**:
   - Animated procedural eyes with realistic blinks, saccades, squints, and pupil dilations.
   - Dynamic animated mouth during speech synthesis.
   - Top status bar: ambient temperature, humidity, and 3 status dots (Wi-Fi, Server, Camera).
   - Bottom focus progress bar and message caption banner.
2. **Room Climate Screen**:
   - Large temperature and humidity readouts from AHT10.
   - Dynamic comfort tips (hydration warnings if dry, ventilation suggestions if humid/hot).
3. **Info & Diagnostics Screen**:
   - Firmware version, brain server URL, last handshake latency, Wi-Fi SSID and RSSI signal strength, local IP address, camera and sensor diagnostics, memory heap/PSRAM, and reset instructions.

### On-Device Web Interface (`http://deskbot.local`)
The ESP32 runs a lightweight embedded HTTP server accessible at `http://deskbot.local` or its local IP:
- Live device telemetry (temperature, humidity, current face, focus time remaining, heap, uptime).
- `/capture`: Real-time camera snapshot test.
- Manual triggers for test faces, buzzer patterns, and focus timer.
- Direct Brain Server URL reassignment.
- Remote trigger to restart into Wi-Fi setup mode.
- `/api/mic` and `/api/pinscan`: In-depth I2S microphone diagnostic endpoints.

---

## AI & Multimodal Engine

### Voice Pipeline

```
[ INMP441 Mic ] ──▶ 16 kHz PCM WAV ──▶ [ ESP32 VAD / Button ] ──▶ [ Server /api/device/voice ]
                                                                             │
                      ┌──────────────────────────────────────────────────────┴───────────────┐
                      ▼                                                                      ▼
            [ English Pathway ]                                                    [ Bangla Pathway ]
                      │                                                                      │
     faster-whisper (local int8 CPU)                                           Google Gemini Multimodal
    Detect wake phrase: "Hey Desky"                                            Audio transcribed directly to
   Deterministic commands vs LLM reasoning                                    Bengali script + contextual reply
                      │                                                                      │
                      ▼                                                                      ▼
             Edge-TTS / pyttsx3                                                     Edge-TTS
           (en-US-AnaNeural voice)                                            (bn-IN-TanishaaNeural voice)
                      │                                                                      │
                      └───────────────────────┬──────────────────────────────────────────────┘
                                              ▼
                             16 kHz mono WAV ──▶ [ MAX98357A Speaker ]
```

- **Hands-Free Wake Detection**: DeskBot continuously listens with adaptive noise thresholds. Speech starting with "Hey Desky", "Desky", or custom aliases (e.g., Bangla "অলি") triggers conversation.
- **English Flow**: Audio is transcribed locally in ~0.4s using `faster-whisper`. Deterministic queries (time, climate, timer) are resolved locally in milliseconds; conversational queries route to Gemini/Claude.
- **Bangla Flow**: Bengali speech streams to Google Gemini, which transcribes directly into Bengali script and provides culturally grounded responses.
- **Text-to-Speech**: High-fidelity neural speech via Microsoft Edge TTS (`en-US-AnaNeural` and `bn-IN-TanishaaNeural`).

### Vision & Distraction Detection
- **Facial Emotion Recognition**: Camera frames are scanned using OpenCV Haar cascades for faces, followed by an ONNX FER+ model evaluating 8 emotions (`neutral`, `happy`, `surprised`, `sad`, `angry`, `disgust`, `fear`, `contempt`).
- **Phone Distraction Alerts**: YOLOv8n scans for COCO class 67 (cell phone). If a phone is detected across multiple consecutive frames during a focus session:
  1. The bot plays a warning buzzer chime.
  2. The screen switches to the `Warning` angry expression.
  3. The bot vocalizes a reminder ("Hey! Phone down, please. You're in focus mode.").
  4. Events are logged to the dashboard timeline.
- **Away Detection**: Noticing when the user has left their desk during focus and welcoming them back on return.

### Offline Resilience Matrix

| Feature | When Offline (No Internet / No AI Key) | Method |
|---|---|---|
| **English Voice STT** | Fully Functional | Local `faster-whisper` (base int8 model) |
| **Bangla Voice STT** | Requires Internet | Gemini Multimodal Audio |
| **English Voice TTS** | Fully Functional | Fallback to Windows SAPI5 (`pyttsx3`) |
| **Bangla Voice TTS** | Text Display Only | Edge-TTS requires network |
| **Time & Date Queries** | Instant & Exact | System clock resolution |
| **Room Climate Queries** | Instant & Exact | Live AHT10 sensor readout |
| **Focus Timer Control** | Fully Functional | Firmware & Server state machines |
| **Volume & Status** | Fully Functional | Local command dispatcher |
| **Small Talk / Jokes** | Pre-scripted Fallback | Built-in bilingual library (`offline.py`) |
| **Facial Animations** | Fully Functional | On-device procedural rendering |
| **Distraction Detection**| Fully Functional | Local YOLOv8n ONNX inference |

---

## Web Dashboard Overview

Access the dashboard from any browser on your network at `http://<server-ip>:8000`:

1. **Home**:
   - Interactive SVG mirror of the bot's animated eyes.
   - Live annotated camera video stream with bounding boxes and emotion probabilities.
   - Environmental widget (temperature and humidity gauges).
   - Quick statistics cards (today's focus minutes, sessions, phone alerts).
2. **Chat**:
   - Full conversational chat log with user and assistant turns.
   - Text input supporting English and Bengali typing.
   - Toggle to read answers out loud through the bot's speaker.
3. **Focus Timer**:
   - Visual Pomodoro countdown ring.
   - One-click Start, Pause, Resume, and Stop controls.
   - Custom session sliders for work and break durations.
4. **Bot Control**:
   - Manual expression switcher (Happy, Sad, Surprised, Love, Sleepy, Focused, etc.).
   - Sound and buzzer test buttons.
   - Remote screen mode switcher (Face / Room / Info).
   - Speech synthesis tester ("Make the bot say...").
   - Live event stream showing phone sightings and system states.
5. **Reports**:
   - **Day View**: Total focus hours, session completion rates, hourly distribution charts, and emotion breakdown.
   - **Week View**: 7-day comparative bar graph, daily averages, week-over-week trends, and consecutive day focus streak.
6. **Settings**:
   - AI provider selection (Gemini or Claude), API keys, and model parameters.
   - Custom bot persona, system prompt rules, and wake word configuration.
   - Voice selection and speech speed rates.
   - Computer vision sensitivity sliders (phone detection confidence, minimum consecutive frame hits, cooldown times).
   - Remote bot management: move bot to a new server IP or trigger remote Wi-Fi setup.

---

## Building the Standalone App

To package the entire Python server, dependencies, UI, and ONNX models into a standalone Windows executable (`DeskBot.exe`) that runs without Python:

1. Ensure the following model files exist in `server/models/`:
   - `emotion-ferplus-8.onnx` (run `python download_models.py`)
   - `whisper-base/` directory (run `python download_models.py`)
   - `yolov8n.onnx` (export from ultralytics):
     ```powershell
     python -c "from ultralytics import YOLO; YOLO('yolov8n.pt').export(format='onnx', imgsz=640, opset=12)"
     move yolov8n.onnx models/
     ```
2. Execute the build batch script:
   ```cmd
   cd server
   build_standalone.bat
   ```
3. PyInstaller will compile the project into `dist/DeskBot/`.
4. Copy the `dist/DeskBot/` folder to any Windows computer and run `DeskBot.exe`.

---

## Codebase Architecture

```
deskbot-with-ai-and-facialexpression/
├── docs/
│   └── WIRING.md                     # Comprehensive wiring pinout and hardware schematics
├── firmware/                         # PlatformIO embedded C++ firmware
│   ├── platformio.ini                # Build definitions, memory flags (qio_opi), huge_app partitions
│   ├── include/
│   │   ├── config.h                  # Hardware pinouts, display drivers, buffer sizes, timing
│   │   └── secrets.example.h         # Optional fallback Wi-Fi credentials template
│   └── src/
│       ├── main.cpp                  # Main setup/loop, button polling, command dispatcher
│       ├── display.cpp               # Procedural eye animation, GFX driver setup, screens
│       ├── audio.cpp                 # Full-duplex I2S audio driver (INMP441 RX, MAX98357A TX)
│       ├── camera.cpp                # ESP32-CAM OV2640 driver initialization and frame capture
│       ├── net.cpp                   # HTTP client streaming frames/audio to brain server
│       ├── wifi_setup.cpp            # SoftAP captive portal and Wi-Fi credential management
│       ├── web_local.cpp             # Embedded web server (http://deskbot.local)
│       ├── aht10.cpp                 # I2C driver for temperature and humidity sensing
│       ├── buzzer.cpp                # PWM tone synthesizer for chimes and alert patterns
│       ├── sfx.cpp                   # Mood chirp synthesizer
│       ├── button.cpp                # Debounced button reader (click, double, long-press)
│       ├── focus_timer.cpp           # On-device Pomodoro timer state machine
│       └── state.cpp                 # Global state structures and event queues
├── server/                           # Python brain server (FastAPI)
│   ├── app.py                        # FastAPI endpoints for device frames, voice, and web UI
│   ├── requirements.txt              # Standard Python dependencies
│   ├── requirements-standalone.txt   # PyInstaller packaging dependencies
│   ├── build_standalone.bat          # Windows batch script to compile DeskBot.exe
│   ├── download_models.py            # Automated downloader for FER+ and Whisper models
│   ├── .env.example                  # Environment configuration template
│   ├── deskbot/
│   │   ├── bot.py                    # Behaviour engine, proactive reactions, audio orchestration
│   │   ├── brain.py                  # LLM integration (Gemini hedged requests, Anthropic Claude)
│   │   ├── vision.py                 # OpenCV Haar cascades, FER+ emotion ONNX, YOLOv8n phone detector
│   │   ├── speech.py                 # faster-whisper STT, Edge-TTS, and Windows SAPI5 audio
│   │   ├── store.py                  # State persistence (data/state.json), stats, chat history
│   │   ├── offline.py                # Deterministic commands and offline small talk library
│   │   ├── report.py                 # Productivity analytics, day/week reports, streak tracking
│   │   └── config.py                 # Path and environment resolution
│   └── static/                       # Web dashboard frontend
│       ├── index.html                # Responsive single-page dashboard layout
│       ├── style.css                 # Dark theme styling, animations, responsiveness
│       └── app.js                    # Real-time state polling, audio recorder, SVG eye mirror
└── README.md                         # Project documentation
```

---

## Troubleshooting & FAQ

| Symptom | Probable Cause | Resolution |
|---|---|---|
| **Bot does not respond to voice** | Mic disabled or hands-free listening inactive | Verify `MIC_ENABLED 1` in `config.h`. Check the Info screen to confirm it shows `ears on`. Ensure your phrase starts with "Hey Desky" (or configured wake word). Open `http://<bot-ip>/api/mic` to verify the microphone data stream is healthy. |
| **Middle dot (Server) is RED** | Bot cannot reach the PC server | 1. Ensure PC and bot are on the same 2.4 GHz Wi-Fi subnet.<br>2. Confirm server is running (`python app.py` or `DeskBot.exe`).<br>3. Check Windows Firewall: allow port 8000 on Private networks.<br>4. Re-enter PC IP at `http://deskbot.local` or via the setup portal. |
| **Left dot (Wi-Fi) is RED** | Wi-Fi connection failure | Hold the button while powering on the bot to enter **DeskBot-Setup** AP (`http://192.168.4.1`). Re-enter Wi-Fi credentials. Note: ESP32 only supports 2.4 GHz networks. |
| **Right dot (Camera) is RED** | Camera hardware fault or pin conflict | Power-cycle the unit. Ensure GPIO 15 is connected exclusively to camera XCLK and GPIO 5 is not pulled down during boot. |
| **TFT screen is blank / white** | Incorrect display driver selected | Check your screen controller. In `firmware/include/config.h`, verify whether your panel is `DISPLAY_ILI9488_480x320` or `DISPLAY_ST7796_480x320`. |
| **Display image is misaligned / skewed** | SPI clock frequency too high for wiring | Lower `TFT_SPI_HZ` in `firmware/include/config.h` from `20000000` (20 MHz) to `16000000` or `10000000`. |
| **Content cut off by enclosure** | Screen margins need adjustment | Adjust `SCREEN_PAD_X` and `SCREEN_PAD_Y` in `config.h` to match your 3D-printed enclosure bezel opening. |
| **Chat displays "Needs AI key"** | Missing Gemini or Claude API key | In the dashboard, navigate to **Settings → AI brain**, paste your API key, and click **Save settings**. |
| **Bangla speech produces no audio output** | Missing internet or API key | Bangla speech transcription and synthesis require an active internet connection. Check the AI status banner in the dashboard. |
| **No phone alerts during study time** | Low confidence or cooldown active | Confirm the focus timer is actively in the `Work` phase. Verify in **Bot → Recent activity** whether "Phone seen" entries appear. Lower `phone_confidence` under **Settings → Fine tuning**. |
| **Firmware flasher fails to connect** | ESP32-S3 not in bootloader mode | Hold down the **BOOT** button on the ESP32-S3 board, tap the **RST** button, then release **BOOT**. Rerun the upload command. |

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
#   d e s k b o t - w i t h - a i - a n d - f a c i a l e x p r e s s i o n  
 