# CLAUDE.md

WiFi-controlled addressable LED controller for ESP32 / ESP8266 (PlatformIO + Arduino framework + FastLED). Serves a web UI and JSON API, persists settings in EEPROM, and supports OTA updates. The original hardware is a dual-ring "RingLight" (141 outer + 126 inner LEDs), but other branches (`led_headphone_stand`, `led_cell`, `stranger_things_led`, `rc_car_led`, `eyemech`, `RingLight`, ...) reuse this codebase for other builds.

## Build / flash

```bash
pio run -e esp32                     # default env (esp32doit-devkit-v1)
pio run -e esp32 -t upload
pio device monitor -b 115200         # 460800 for esp32c3 / esp32s3 envs
```

Environments in [platformio.ini](platformio.ini): `esp32`, `esp8266`, `esp32c3`, `esp32s3`, `seeed_xiao_esp32s3`. CI ([.github/workflows/main.yml](.github/workflows/main.yml)) builds `esp32c3`, `esp32`, `esp8266` — and only on pushes to the feature branches, not `main`. There are no unit tests (`test/` is empty); verification means building every affected env and testing on hardware.

`buildscript_versioning.py` runs pre-build and rewrites [include/version.h](include/version.h) (`SEMVER` env var, else `dev.<timestamp>`). That file is tracked but changes on every build — don't commit it as part of unrelated changes.

## Architecture

- [src/main.cpp](src/main.cpp) — `setup()`/`loop()` only. Creates `CDevice`, `CWifiManager`, `CLEDManager`, wires callbacks, handles the factory-reset smooth-boot timer.
- [src/Configuration.h](src/Configuration.h) / [.cpp](src/Configuration.cpp) — **compile-time feature flags** (`WIFI`, `LED`, `OLED`, `BUTTONS`, `KEYPAD`, `RING_LIGHT`, `WEB_LOGGING`, `DISABLE_LOGGING`), per-chip pin/name defaults (`CONFIG_IDF_TARGET_ESP32C3/S3/ESP32`, `ESP8266`), the global `configuration_t configuration`, EEPROM load/save/wipe, and `CONFIG_getLedBrightness()` (applies the power-save hour window using NTP time).
  - `Configuration.h.ringlight` / `Configuration.h.ledstrip` are older alternative profiles to copy over `Configuration.h`; they lag behind the current file.
- [src/LedManager.cpp](src/LedManager.cpp) — allocates `leds[configuration.ledStripSize]`, picks the FastLED chipset from `configuration.ledType` (0–13 switch), **registers modes in `registerModes()`**, renders the current mode, handles auto-cycling and the button-driven charging mode. Custom palettes (PayPal, Pride, Christmas, SlavaUkraini) live here.
- [src/modes/](src/modes/) — each mode extends `CBaseMode` and implements `draw(CRGB *leds)`, called every loop. `CLEDSegment` + `CRingPaletteMode` split the strip into independently-animated ranges (uses `OUTTER_RING_SIZE`).
- [src/wifi/WifiManager.cpp](src/wifi/WifiManager.cpp) — STA connect with soft-AP fallback (`<DEVICE_NAME>` + chip id, `192.168.4.1`), ESPAsyncWebServer routes, NTP via POSIX TZ strings, ElegantOTA at `/update`. HTML/CSS are string constants in [HTMLAssets.cpp](src/wifi/HTMLAssets.cpp).
- [src/Device.cpp](src/Device.cpp) — device state machine + optional SSD1306 OLED (72x40 on C3/S3 boards, drawn through a scrolling 128x40 virtual canvas).
- [src/BaseManager.h](src/BaseManager.h) — shared base interface carried over from sibling projects; mostly unused here.

### Web routes
`/` (main UI), `/wifi`, `/device`, `/style.css`, `/log` (if `WEB_LOGGING`; reading clears the buffer), `POST /factory_reset`, `POST /reboot`, `/update` (OTA).
- `GET /config` returns full config JSON; `POST /config` (JSON body) applies it, **saves to EEPROM and reboots**.
- `GET /api` returns LED state; `POST /api` applies JSON live **without saving to EEPROM**.
- All JSON field handling is in `CWifiManager::updateConfigFromJson()`; some fields (WiFi, NTP, `ledType`) set `rebootNeeded`.

## Conventions and gotchas

- **`configuration_t` is raw-`EEPROM.put` serialized.** Adding/reordering/resizing fields shifts the layout and corrupts existing devices' settings. Append new fields at the end (before `_loaded`), give them defaults in `EEPROM_loadConfig()`, and add a sanity check like the existing `isnan`/range checks. The `_loaded == "jaisor"` magic string marks a valid config.
- Factory reset: 3 boots without surviving `FACTORY_RESET_CLEAR_TIMER_MS` (2s) wipes EEPROM. Counter is EEPROM byte 0; config starts at byte 1.
- Brightness is applied only via `FastLED.show(255 * CONFIG_getLedBrightness())` in `LedManager`. Modes must not call `FastLED.setBrightness()` or `FastLED.show()`.
- Modes must be non-blocking: use the `tMillis` + `configuration.ledDelayMs` pattern, never `delay()`. Use `numLeds` (= runtime `ledStripSize`) for bounds, not `LED_STRIP_SIZE`.
- Adding a mode: create `src/modes/XMode.{h,cpp}` extending `CBaseMode`, include it in `LedManager.cpp`, `push_back` it in `registerModes()`. Mode indices are persisted (`ledMode`, `cycleModesList`), so inserting/reordering changes what users have saved — append at the end where possible.
- Platform code is guarded with `#if defined(ESP32)` / `#elif defined(ESP8266)`; chip-specific settings use `CONFIG_IDF_TARGET_*`. Any change should compile for all envs, with feature flags both on and off (e.g. `OLED`, `RING_LIGHT`, `WEB_LOGGING`).
- Logging uses ArduinoLog (`Log.infoln`, `Log.verboseln`, ...). On C3/S3 USB-CDC boards, `while (!Serial)` in `setup()` can block when no host is attached.
- Style: classes prefixed with `C` (`CLEDManager`, `CPaletteMode`), 2-space indent in most files, `#pragma once` headers.
- [.github/copilot-instructions.md](.github/copilot-instructions.md) is partially stale (mentions `/api/led`, `/api/device`, and mode registration in `main.cpp`); trust the code over it.
