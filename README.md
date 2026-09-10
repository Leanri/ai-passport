<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Egg Catcher for FoloToy AI Passport

A compact egg-catching game inspired by classic handheld LCD games. It runs directly after boot and uses tiny one-bit pixel sprites, so no large image assets are stored in Flash or RAM.

## Controls

- `UP`: turn the wolf and basket to the left chicken.
- `DOWN`: turn the wolf and basket to the right chicken.
- Press either button on the start or game-over screen to play.
- Hold `OK`: leave the game and return to the title screen.
- Press `OK` on the title screen to play again.

The game keeps score, allows three misses, accelerates as the score rises, and shows battery percentage when the fuel gauge is available.

## Hardware target

- FoloToy AI Passport with ESP32-C3
- 8 MB Flash, no PSRAM
- 240 × 320 ST7789P3 display
- Three ADC buttons (`UP`, `DOWN`, and `OK`)

The original 3 MB application limit and protected `cardid` partition at `0x356000` remain unchanged. Never erase the entire Flash of a provisioned device.

## Build and flash

Use ESP-IDF 5.5.3. The verified firmware image is produced as `build/FoloToy-AI-Passport-full.bin`. For a provisioned device, prefer segmented `idf.py flash` so the protected identity region is not touched.

See the upstream [build guide](docs/development/engineering/build-and-test.md) for the exact commands and safety checks.
