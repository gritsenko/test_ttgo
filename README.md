# test_ttgo

PlatformIO samples for TTGO T-Display v1.1 (ESP32, ST7789 135x240).

## Quick Start

- Build default sample:
  - `platformio run`
- Upload default sample:
  - `platformio run --target upload`
- Monitor serial:
  - `platformio device monitor`

Current default environment in `platformio.ini` is `snake`.

## Samples

### snake (default)

Snake with continuous circular movement on a wrapping playfield.

- Display: portrait mode, 135x220 playfield below the score bar
- Either button (GPIO0 or GPIO35): reverse angular velocity on each press
- Holding a button does not repeat the turn; release and press again
- Movement speed and turn radius stay constant (23 pixels)
- Cross any edge to appear on the opposite side, including food and body collisions
- Collect orange food to grow; hitting your own body ends the game
- Press either button after game over to restart
- A new best score is saved to ESP32 NVS at game over or victory and restored
  after restart or power loss; scores from unfinished games are not saved
- Battery icon and estimated charge percentage in the top bar (GPIO34 ADC,
  divider enabled via GPIO14); averaged and smoothed readings every 1.5 seconds
- Battery icon is red at 20% or less, yellow up to 50%, green above 50%

Build or upload this sample explicitly:
- `platformio run -e snake`
- `platformio run -e snake --target upload`

### racing_sample

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
