# test_ttgo

PlatformIO samples for TTGO T-Display v1.1 (ESP32, ST7789 135x240).

## Quick Start

- Build default sample:
  - `platformio run`
- Upload default sample:
  - `platformio run --target upload`
- Monitor serial:
  - `platformio device monitor`

Current default environment in `platformio.ini` is `racing_sample`.

## Samples

### racing_sample (default)

Top-down portrait racing game inspired by classic brick games.

- Display: portrait mode
- Controls:
  - Left button: move left
  - Right button: move right
  - Both buttons: nitro boost
- Nitro:
  - Cyan nitro pickups spawn on track
  - Collect pickup to refill nitro gauge
  - Nitro is consumed while both buttons are held
- Restart after crash:
  - Hold both buttons

Build only this sample:
- `platformio run -e racing_sample`

### face_sample

Simple robot-face animation with battery indicator.

Build only this sample:
- `platformio run -e face_sample`

### seats_sample

Fetches seats JSON over Wi-Fi and renders several lines on screen.

Build only this sample:
- `platformio run -e seats_sample`

## Notes

- For `seats_sample`, configure Wi-Fi in `include/local_secrets.h`.
- Board definition is in `boards/lilygo-t-display-v11.json`.
