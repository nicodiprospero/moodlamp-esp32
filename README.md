<div align="center">

# MoodLamp ESP32

**A Wi-Fi mood lamp on a single ESP32-S3 with one WS2812B LED. Six animated modes, smooth fades and a self-hosted web UI. No app, no cloud.**

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-N16R8-E7352C?logo=espressif&logoColor=white)
![WS2812B](https://img.shields.io/badge/LED-WS2812B-ff69b4)
![Arduino](https://img.shields.io/badge/Arduino-core%202.0.17-00979D?logo=arduino&logoColor=white)
![License](https://img.shields.io/badge/license-MIT-blue)

</div>

---

The ESP32-S3-N16R8 dev board ships with an addressable RGB LED on GPIO 48. MoodLamp turns that LED into a phone-controlled lamp. It's a compact example of the pattern I use across my IoT projects: **access point + embedded web server + JSON state endpoint + non-blocking animation loop**.

## Features

- **6 modes:** `off`, `solid`, `breathe`, `rainbow`, `fire` (randomized flicker targets), `party` (fast hue cycling)
- **Smooth color transitions:** solid mode lerps from the current color to the target instead of jumping
- **Brightness and speed** sliders (0–100)
- **Color picker** for the base color used by solid and breathe
- **Non-blocking loop:** animations use `millis()` timing, so the web server never stalls
- **Live UI:** the page polls `/state` and reflects the active mode, color and glow

## Endpoints

| Endpoint | Params | Effect |
|---|---|---|
| `GET /` | — | web UI |
| `GET /color` | `r`, `g`, `b` (0–255) | set base color |
| `GET /bright` | `v` (0–100) | brightness |
| `GET /mode` | `v` = `off` · `solid` · `breathe` · `rainbow` · `fire` · `party` | switch mode |
| `GET /speed` | `v` (0–100) | animation speed |
| `GET /state` | — | JSON with the current state |

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3-N16R8 dev board | onboard WS2812B on **GPIO 48** |
| (optional) external WS2812B ring/strip | change `LED_PIN` / `LED_COUNT` |

## Getting started

1. Arduino IDE 2 → Boards Manager → **esp32 by Espressif Systems** (tested on 2.0.17).
2. Library Manager → **Adafruit NeoPixel**.
3. Open `firmware/MoodLamp/MoodLamp.ino`, board *ESP32S3 Dev Module*, upload.
4. Join Wi-Fi **`MoodLamp`** and open **http://192.168.4.1**.

> The AP password in the sketch is a placeholder (`12345678`). Change `AP_PASS` before you keep it on your desk.

## Roadmap

- [ ] Station mode (join home Wi-Fi) with mDNS `moodlamp.local`
- [ ] Persist last mode/color in NVS
- [ ] Multi-LED ring support with per-pixel effects
- [ ] MQTT / Home Assistant integration

---

<sub>Built by **Nicolas Di Prospero** · IoT & security · [github.com/nicodiprospero](https://github.com/nicodiprospero)</sub>
