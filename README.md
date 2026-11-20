# DeskBot

A study-buddy robot for your desk, inspired by Emo. It runs on an **ESP32-S3 CAM** with a 3.5" screen,
reads your face, keeps you focused, catches you using your phone, and chats with you in **English or Bangla**.

- **Expressive face**: animated eyes on the TFT (happy, sad, grumpy, surprised, sleepy, love, focused, confused, warning).
- **Mood sounds**: short robot chirps on the speaker whenever its expression changes.
- **Reads your mood**: the camera detects your facial emotion; the bot reacts and sometimes says something kind.
- **Focus timer**: Pomodoro-style focus and break sessions, from the button or the dashboard.
- **Phone warnings**: during focus time it beeps, frowns and tells you to put the phone down, and keeps reminding you.
- **Room climate**: live temperature and humidity (AHT10), with tips when the room is too hot, humid or dry.
- **Talk by voice, hands-free**: always listening; start with the wake phrase ("Hey Oly, what time is it?") and it
  answers out loud. The button works too.
- **Reports**: every focus session is logged; the bot speaks a summary when a session ends, and the dashboard
  shows day and week reports (focus time, sessions, phone warnings, mood, streak).
- **Chat**: type to the bot in English or Bangla; it answers on screen and out loud (free Gemini key).
- **Works offline too**: time, date, room, timer, volume and common questions are answered without internet.
- **Web dashboard**: Home, Chat, Focus, Bot and Settings tabs, in English or বাংলা.

```
 ESP32-S3 CAM (firmware/)                         PC on the same Wi-Fi (server/)
 ─────────────────────────                        ──────────────────────────────────────
 camera + sensors, every second  ───────────────▶  face emotion (OpenCV + FER+)
 animated face, buzzer, button                     phone detection (YOLOv8n)
 focus timer, speaker                              chat brain (Gemini or Claude)
                                ◀───────────────   speech (English + Bangla voices)
                                  say / face /     dashboard at http://<pc-ip>:8000
                                  beep / timer
```

The ESP32 handles the hardware and animation. The DeskBot server on a PC does the AI work.

---

## What you need

| Part | Notes |
|---|---|
| ESP32-S3 CAM board (N16R8 or N8R8, OV2640 camera) | octal PSRAM required |
| 3.5" SPI TFT, 480x320 (ILI9488) | ST7796, ST7789, ILI9341 and ST7735 also supported |
| MAX98357A I2S amplifier + 4-8 Ω speaker | voice and mood sounds |
| AHT10 temperature/humidity sensor | |
| 2-pin buzzer, 2-pin push button | |
| INMP441 I2S microphone | for talking by voice |
| A Windows PC or laptop on the same Wi-Fi | runs the DeskBot server |
| A free Gemini API key | [aistudio.google.com/apikey](https://aistudio.google.com/apikey) |

---

## Setup guide

Five steps: wire the bot, flash it, start the server, connect it to Wi-Fi from your phone, add your AI key.

### Step 1. Wire the bot

Follow **[docs/WIRING.md](docs/WIRING.md)**. The short version:

| Module | Pins (ESP32-S3) |
|---|---|
| TFT | MOSI 47, SCLK 48, CS 39, DC 40, RST 41, VCC/BL 3V3 |
| MAX98357A amp | BCLK 1, LRC 2, DIN 3, VIN 5V |
| AHT10 | SDA 42, SCL 0 |
| Buzzer | 38 and GND |
| Button | 45 and GND |
| INMP441 mic | SCK 1, WS 2, SD **5**, L/R GND, VDD 3V3 |

### Step 2. Flash the bot

You need **VS Code** with the **PlatformIO IDE** extension. You do **not** need to put your Wi-Fi in the code.

1. Check `firmware/include/config.h`:
   - `DISPLAY_ILI9488_480x320` is selected for a 3.5" 480x320 screen. Pick another `DISPLAY_*` line if yours differs.
   - `SCREEN_PAD_X` / `SCREEN_PAD_Y` keep the picture clear of the enclosure's edges, and `SCREEN_FRAME_R`
     sets the corner radius of the frame drawn around it.
   - If the board has **8 MB** flash, set `board_upload.flash_size = 8MB` in `firmware/platformio.ini`.
2. Connect the board with the USB port marked **UART/COM** (CH340/CH343).
   In VS Code open the `firmware` folder and click **PlatformIO: Upload** (the → arrow in the bottom bar).
   If the upload can't connect, hold **BOOT**, tap **RST**, release **BOOT** and try again.

### Step 3. Start the server on the PC

Start it now, so you know the PC address for the next step.

Pick one.

**Option A: the ready-made app (easiest, no Python needed)**

1. Copy the `dist\DeskBot` folder to the PC (build it once with `server\build_standalone.bat`, see
   [Build the standalone app](#build-the-standalone-app)).
2. Double-click `DeskBot.exe`. A console window opens and the dashboard opens in the browser.
3. If Windows asks whether DeskBot may use the network, click **Allow** for the network type your Wi-Fi uses
   (home Wi-Fi is marked either Private or Public); otherwise the bot can't reach it.

Close the console window to stop the server. Settings and stats are saved in the `data` folder next to `DeskBot.exe`.

**Option B: from the source code (for development)**

Needs **Python 3.10, 3.11 or 3.12** ([python.org](https://www.python.org/downloads/), tick "Add Python to PATH").
Open a terminal in the `server` folder and run, one at a time:

```bash
python -m venv .venv
```

```bash
.venv\Scripts\python.exe -m pip install -r requirements.txt
```

```bash
.venv\Scripts\python.exe download_models.py
```

```bash
copy .env.example .env
```

```bash
.venv\Scripts\python.exe app.py
```

`download_models.py` also downloads the speech model (about 140 MB). The install downloads about 2 GB the first time. When the window shows `Uvicorn running on http://0.0.0.0:8000`
the server is ready. Keep the window open. Allow Python through the firewall the same way as in Option A.

### Step 4. Connect the bot to your Wi-Fi and the server

The server window shows the PC address, for example `Bot server URL: http://192.168.0.111:8000`.

1. Power the bot on. With no Wi-Fi saved, its screen shows **Wi-Fi setup** and it creates its own Wi-Fi called
   **DeskBot-Setup** (password **11221122**, also shown on the screen).
2. On your phone (or laptop), join **DeskBot-Setup** with the password **11221122**. The setup page opens by itself.
   If it doesn't, open **http://192.168.4.1** in the browser.
3. Tap your Wi-Fi in the list, type its password, and enter the PC address (just the IP, like `192.168.0.111`, is enough).
4. Press **Save and connect**. The bot restarts and joins your Wi-Fi. Switch your phone back to your normal Wi-Fi.
5. The bot's screen shows the face, and the three dots at the top right (Wi-Fi, server, camera) turn green.

The ESP32 only works with **2.4 GHz** Wi-Fi. A phone hotspot works too; the PC must then join the same hotspot.

**Changing Wi-Fi or the PC later**

The easiest way is the dashboard: **Settings → Bot connection** shows the bot's address, its Wi-Fi and its server.

- **Move to another PC:** start DeskBot on the new PC, type its address (shown in its window) into
  **Move the bot to another server** and press send. The bot checks the new address first; if it can't reach it,
  it stays on the current server.
- **Change Wi-Fi:** press **Open Wi-Fi setup**. The bot restarts as **DeskBot-Setup** (password 11221122);
  join it from your phone and pick the new network. If nobody does within 5 minutes, it rejoins the old Wi-Fi.

Without the dashboard (for example, the old PC is gone):

- **Hold the button while switching the bot on** (about 2 seconds) to open DeskBot-Setup. There you can set
  both the Wi-Fi and the PC address.
- Or open **http://deskbot.local** (the bot's own page) and use **Brain server URL** or **Change Wi-Fi**.
- If the saved Wi-Fi is out of range, the bot opens DeskBot-Setup for 3 minutes, then tries the saved Wi-Fi again.
- To change the setup network's name or password, edit `SETUP_AP_NAME` / `SETUP_AP_PASSWORD` in `config.h`
  (the password needs at least 8 characters) and upload again.

### Step 5. Add your AI key

1. Open the dashboard: **http://localhost:8000** on the PC, or **http://&lt;pc-ip&gt;:8000** from a phone on the same Wi-Fi.
2. Get a free key at [aistudio.google.com/apikey](https://aistudio.google.com/apikey).
3. Go to **Settings → AI brain**, choose **Gemini**, paste the key and press **Save settings**.
4. Open **Chat** and say hello, in English or Bangla.

The key is stored only on the PC and is never shown in the dashboard again. To use Claude instead,
choose **Claude** in the same section and paste a Claude API key.

### Check that everything works

| Check | Where |
|---|---|
| All three dots on the bot are green | bot screen |
| The bot's eyes move on the Home tab | dashboard → Home |
| Temperature and humidity show numbers | dashboard → Home |
| The camera shows a picture and your mood | dashboard → Home |
| A message gets a reply | dashboard → Chat |
| Holding the button starts a focus session | bot + dashboard → Focus |

---

## Using DeskBot

### The button

| Press | What happens |
|---|---|
| Double-click | Switch screen: **Face → Room → Info → Face** (the dots at the top show which one) |
| Hold about 1 second | Start or stop a focus session |
| Click | Pause or resume a running focus session. Otherwise: talk by voice (with a working mic) |
| Hold 5 seconds **on the Info screen** | **Reset everything**: forgets Wi-Fi, server and settings, then opens DeskBot-Setup |
| Hold while switching on | Open DeskBot-Setup to change Wi-Fi or the PC address |

### The bot's screens

- **Face**: the animated eyes, with temperature/humidity and the Wi-Fi, server and camera dots at the top.
- **Room**: large temperature and humidity.
- **Info**: whether the bot is connected to a server (and when it last was), the server address, Wi-Fi name and
  signal, the bot's IP, camera/sensor/mic status, uptime and firmware version.

The reset asks you to keep holding and counts down from 5; let go at any time to cancel.

### Dashboard tabs

| Tab | What's there |
|---|---|
| **Home** | The bot's live eyes, the camera view with your detected mood, temperature, humidity, today's stats |
| **Chat** | Type to the bot. "Read replies out loud on the bot" makes it speak the answer |
| **Focus** | Timer ring with one play/pause button and stop, focus and break lengths |
| **Bot** | Expressions, mood sounds on/off, volume, test sounds, screen mode, make the bot say exact words, recent activity |
| **Settings** | AI key and model, bot name, language, English and Bangla voices, personality, behaviour switches |

**EN / বাংলা** at the top right switches the dashboard language.

### Language

- **Listening:** the bot always understands both. English speech is recognised on your PC (works offline).
  Bangla speech is recognised and answered by Gemini in one step, so Bangla voice needs internet and the Gemini key.
  Say the wake phrase in either language: "Hey Emo, what time is it?" or "হেই ইমো, এখন কয়টা বাজে?".
- **Settings → Bot speaks**: *Whatever I use* answers in the language you spoke, or fix it to English or বাংলা.
- English replies are spoken by **Ana**, Bangla replies by **Tanishaa** (online voices).
- The bot's screen can't draw Bangla letters, so Bangla captions show "( Bangla )" there while the bot speaks them.

### Things to try in Chat

- "Start a 40 minute focus session" / "pause the timer" / "stop focus"
- "Set the volume to 60"
- "How hot is the room?"
- "আমার পড়ায় মন বসছে না"

### Phone warnings

During a focus session, a phone seen in 2 of the last 5 camera frames triggers a warning: buzzer, warning face and a
spoken reminder, repeated every 45 seconds while the phone stays in view. Each sighting appears in
**Bot → Recent activity** as "Phone seen (xx%)". Sensitivity is under **Settings → Fine tuning**.

---

## Microphone

The INMP441 is wired to **SD = GPIO5** (`MIC_ENABLED 1`, `I2S_DIN_PIN 5` in `config.h`).
GPIO5 is also the camera's setup line; the firmware sets up the camera first and then hands the pin to the mic,
so both work.

- **Hands-free with a wake phrase (default):** the bot is always listening, but like other voice assistants it only
  answers when your sentence **starts** with the wake phrase, **"Hey <bot name>"** (for example "Hey Oly, what time is
  it?"). Just the name at the start ("Oly, ...") also works, because speech recognition often drops "hey".
  Say only the wake phrase and it replies "Yes?" and waits a few seconds for your question. Anything that doesn't
  start with the wake phrase is ignored. Audio only goes to your own PC.
  Change the phrase in **Settings → Behaviour → Wake phrase**, add other spellings (for example the Bangla অলি) under
  **Extra wake words**, or switch listening off with **Hands-free**.
- **Button:** click (when no focus session is running) to ask without the name. The bot beeps and listens, stops when
  you go quiet (or click again), then answers out loud.
- **Speed:** a spoken answer takes about 3–4 seconds.
- **Check the mic:** open `http://<bot-ip>/api/mic`. A working mic shows `allOnes` near 0 and a wide spread between
  `min` and `max`.
- Uploading a different sketch (for example a mic test) erases DeskBot's saved Wi-Fi and server. After flashing
  DeskBot again, join **DeskBot-Setup** once to set them back.

## Reports

Every focus session is saved (start, end, minutes focused, phone warnings, times you left the desk, your main mood).

- **When a session ends** the bot says a short summary, for example: "Session done! 25 minutes of focus. No phone at
  all, great job! You looked mostly calm. Today so far: 3 sessions, 75 minutes. Take a 5 minute break."
  (English or Bangla, works offline.) A session you stop early is summarised too and marked "stopped early".
- **Dashboard → Reports**:
  - **Day**: focus time, sessions, phone warnings, completion rate, focus by hour, mood while focusing, and the list
    of sessions. Use the arrows to look at earlier days.
  - **Week**: 7-day bar chart, totals, average per day, change versus the previous week, best day, streak, mood and
    sessions. The arrows go back week by week.
- **By voice**: "Hey Oly, how did I do today?" / "how did I do this week?" / "এই সপ্তাহে কেমন পড়েছি?"

History is kept in `server/data/state.json` (the last 1000 sessions).

## Is the AI connected?

- **Dashboard header:** under the bot's status it shows the AI status, for example "Gemini connected", "Gemini no
  internet" or "Gemini no API key".
- **Settings → AI brain:** the same status with the last reply time, and a **Test connection** button.
- **On the bot:** double-click to the **Info** screen; the **AI** line says Connected, No API key, No internet or Busy.
  "ears on" means hands-free listening is active.

When the AI isn't connected, the bot still answers the built-in questions below.

## Offline mode

DeskBot keeps working when the internet or the AI key is missing:

| Works offline | How |
|---|---|
| Speech recognition (English) | local model in `server/models/whisper-base` |
| Speech recognition (Bangla) | needs internet: Gemini understands Bangla speech much better than local models |
| English voice | Windows' built-in voice (Zira) only when there's no internet; otherwise always Ana |
| Time, date, room temperature and humidity | answered by the server itself |
| Focus timer: start (with minutes), pause, resume, stop, time left | answered by the server itself |
| Volume up/down/set, today's stats, "how do I look" | answered by the server itself |
| Small talk: hello, how are you, who are you, what can you do, jokes, study tips, encouragement, tired, sad, breathing exercise, thanks, bye | built-in answers, used when the AI can't answer |
| Face, mood sounds, phone warnings, focus timer on the bot, room screen | always local |

All built-in answers exist in **English and Bangla**. Questions like "What time is it?" and "কয়টা বাজে?" are answered
instantly even when the AI is available. Anything else goes to the AI; offline, the bot says what it can still do.
Bangla replies need internet to be spoken (Windows has no offline Bangla voice); offline they appear as text in the dashboard.

---

## Build the standalone app

Run this once on a PC that has the project and Python 3.10–3.12. The result runs anywhere without Python.

1. Make sure `server/models` contains `emotion-ferplus-8.onnx` (from `download_models.py`) and `yolov8n.onnx`.
   To create `yolov8n.onnx`, with the Option B environment set up, run in the `server` folder:

   ```bash
   .venv\Scripts\python.exe -c "from ultralytics import YOLO; YOLO('yolov8n.pt').export(format='onnx', imgsz=640, opset=12)"
   ```

   then move `yolov8n.onnx` into `server/models`.
2. Double-click `server/build_standalone.bat`. It creates its own small build environment (no PyTorch)
   and writes the app to `dist\DeskBot\` (about 550 MB, including the speech model).

The ready-made app starts with empty settings, so add the AI key in Settings on each new PC.

---

## Project layout

```
firmware/                     PlatformIO project (Arduino core 2.0.17)
  include/config.h            pins, display type, padding, mic switch
  include/secrets.example.h   optional default Wi-Fi/server (normally set in DeskBot-Setup)
  src/main.cpp                start-up, button, focus events, commands from the server
  src/display.cpp             animated face, status bar, captions, focus bar
  src/sfx.cpp                 mood sounds (synthesized chirps)
  src/audio.cpp               I2S speaker (and mic when enabled)
  src/net.cpp                 sends frames + sensors, receives commands, plays speech
  src/web_local.cpp           the bot's own page: http://deskbot.local
  src/wifi_setup.cpp          DeskBot-Setup Wi-Fi portal
  src/camera.cpp, aht10.cpp, buzzer.cpp, button.cpp, focus_timer.cpp, state.cpp
server/
  app.py                      web server and API
  deskbot/bot.py              behaviour: phone warnings, mood reactions, timer events, chat
  deskbot/brain.py            Gemini / Claude, language handling
  deskbot/vision.py           face, emotion and phone detection (ONNX)
  deskbot/speech.py           text-to-speech (and speech-to-text for the mic)
  deskbot/store.py            settings, stats, chat history (data/state.json)
  deskbot/report.py           day and week reports
  deskbot/offline.py          built-in answers (English + Bangla), work without internet
  static/                     dashboard (HTML, CSS, JS)
  build_standalone.bat        builds dist\DeskBot\DeskBot.exe
docs/WIRING.md                pin-by-pin wiring and pin warnings
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| The bot doesn't react when I talk to it | Check that **Hands-free** is on and the Info screen says "ears on". Start with the wake phrase, clearly ("Hey Oly, ..."). Short names are hard for speech recognition: add how it gets misheard, or a Bangla spelling, under **Extra wake words**. |
| Not sure if the bot reaches the server | Double-click twice to open the **Info** screen: it says Connected or when it last was. |
| Wi-Fi dot is red, or the screen shows "Wi-Fi setup" | Join **DeskBot-Setup** (password 11221122) from your phone and choose your Wi-Fi again (check the password). The ESP32 only joins 2.4 GHz Wi-Fi. |
| The setup page doesn't open on the phone | Open http://192.168.4.1 manually. Turn off mobile data if the phone keeps leaving DeskBot-Setup. |
| Server dot is red after the PC restarted | The PC's address may have changed (`ipconfig`). Open http://deskbot.local and set **Brain server URL**, or use Settings → Bot connection from the dashboard. |
| Server dot is red | Is the server window open? Does the bot's Brain server URL (http://deskbot.local) match the address the server prints? Same Wi-Fi? Allow DeskBot.exe (or Python) through Windows Firewall for your Wi-Fi's network type. |
| Camera dot is red, or no camera picture | Power-cycle the bot. Make sure nothing else is wired to GPIO15 or GPIO5. |
| Screen is white | Wrong display driver: pick the right `DISPLAY_*` in `config.h`. A 3.5" 480x320 "v1.0" module is usually ILI9488, sometimes ST7796. |
| Picture slides sideways on the screen | Lower `TFT_SPI_HZ` to `20000000` in `config.h`. |
| Parts of the screen hidden by the enclosure | Increase `SCREEN_PAD_X` / `SCREEN_PAD_Y` (now 34 / 30). |
| Chat says it needs an AI key | Settings → AI brain → paste the key → Save settings. |
| Chat is slow or says it's busy | Free Gemini models get busy. The server tries backup models for up to 12 seconds, then answers "my AI is busy" and offers what it can do offline. The dashboard header shows the AI status. |
| The bot doesn't understand my Bangla | Bangla speech goes to Gemini: check the AI status in the dashboard header and the internet. Set **Bot speaks** to *Whatever I use*. |
| Chat replies in the wrong language | Settings → Bot speaks → *Whatever I use*, or pick a fixed language. |
| No phone warnings | Is a focus session running? Does "Phone seen" appear in Bot → Recent activity? If not, the camera can't see the phone; lower "Phone confidence" in Fine tuning. |
| No sound | Check the speaker wiring and Bot → volume. Mood sounds can be switched off in Bot → Sound. |
| Dashboard looks old after an update | Reload the page once. |
| Upload fails or the COM port disappears | Try another USB cable or port, or hold BOOT while tapping RST. |
#   d e s k b o t - w i t h - a i - a n d - f a c i a l e x p r e s s i o n  
 