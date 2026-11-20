# Wiring - ESP32-S3 CAM

All modules run on **3.3 V** logic. Power the MAX98357A from **5 V** for more volume.

## Pin table

| Module | Module pin | ESP32-S3 GPIO | Notes |
|---|---|---|---|
| **INMP441 mic** | SD | **5** | shared with the camera setup line, see note 1. **Never 15** (camera clock) |
| | SCK | **1** | shared with amp BCLK |
| | WS | **2** | shared with amp LRC |
| | L/R | GND | left channel |
| | VDD / GND | 3V3 / GND | |
| **MAX98357A amp** | LRC | **2** | shared with mic WS |
| | BCLK | **1** | shared with mic SCK |
| | DIN | **3** | |
| | GAIN | not connected | 9 dB (tie to GND = 12 dB, to VIN = 6 dB) |
| | SD | not connected | on, mono (L+R)/2 |
| | VIN / GND | 5V / GND | speaker 4-8 Ω on + / - |
| **AHT10** | SDA | **42** | |
| | SCL | **0** | |
| | VIN / GND | 3V3 / GND | |
| **TFT (SPI)** | MOSI / SDA | **47** | |
| | SCLK / SCL | **48** | |
| | CS | **39** | |
| | DC | **40** | |
| | RST | **41** | |
| | BL / LED | 3V3 | always on |
| | VCC / GND | 3V3 / GND | |
| **Buzzer** (2-pin) | + | **38** | passive buzzer recommended |
| | - | GND | |
| **Push button** (2-pin) | one leg | **45** | internal pull-up |
| | other leg | GND | |

The camera uses its own connector (XCLK 15, SIOD 4, SIOC 5, D0-D7 11/9/8/10/12/18/17/16,
VSYNC 6, HREF 7, PCLK 13). If your board's camera pinout is different, edit the `CAM_PIN_*`
lines in `firmware/include/config.h`.

## Why the mic and amp share pins 1 and 2

Both run on the **same I2S peripheral**: the ESP32 drives one bit clock (GPIO1) and one word-select
(GPIO2); the amp receives data on GPIO3 and the mic sends data on GPIO5.
This is intentional and saves pins.

## Things to know

0. **Never put the mic SD on GPIO15** - it is the camera's 20 MHz clock output (XCLK).
   Two outputs on one wire can damage the mic or the pin. GPIO5 (current wiring), 14 or 21 are fine; set
   `I2S_DIN_PIN` and `MIC_ENABLED 1` in `config.h`. Test it at `http://<bot-ip>/api/mic`
   (working mic: `allOnes` well below 8192).
1. **GPIO5 is shared with the camera's setup line (SCCB SIOC).** The firmware configures the camera first,
   then releases GPIO5 to the mic, so both work. Camera settings can't be changed after start-up.
2. **GPIO0 (AHT10 SCL)** is the BOOT strapping pin. The AHT10 module's pull-up keeps it HIGH, so the
   board boots normally. Holding the board's BOOT button will disturb the sensor - harmless.
3. **GPIO48** drives the onboard RGB LED on many S3 boards. Using it as TFT SCLK can make the LED
   flicker; the display still works.
4. **GPIO45** is a strapping pin for flash voltage. The button pulls it LOW, which is the default,
   so pressing it during boot is safe.
5. GPIO35-37 are used by octal PSRAM - don't wire anything there.

## Display driver

Default is **ILI9488 480x320** (3.5" SPI module), landscape, with a 20 px margin for the enclosure. If your TFT is different, change the `DISPLAY_*` define in
`config.h` (ST7789 240x320 / 135x240, ILI9341 240x320 and ST7735 128x160 are prepared).
If colours look inverted, flip the `ips` argument (`true`/`false`) for your panel in
`firmware/src/display.cpp`.
