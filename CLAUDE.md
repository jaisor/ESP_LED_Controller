# CLAUDE.md

WiFi-controlled addressable LED controller for ESP32 / ESP8266 (PlatformIO + Arduino framework + FastLED). Serves a web UI and JSON API, persists settings in EEPROM, and supports OTA updates. The original hardware is a dual-ring "RingLight" (141 outer + 126 inner LEDs), but other branches (`led_headphone_stand`, `led_cell`, `stranger_things_led`, `rc_car_led`, `eyemech`, `RingLight`, ...) reuse this codebase for other builds.

## Build / flash

```bash
pio run -e esp32                     # default env (esp32doit-devkit-v1)
pio run -e esp32 -t upload
pio device monitor -b 115200         # 460800 for esp32c3 / esp32s3 envs
```

Environments in [platformio.ini](platformio.ini): `esp32`, `esp8266`, `esp32c3`, `esp32s3`, `seeed_xiao_esp32s3`. CI ([.github/workflows/main.yml](.github/workflows/main.yml)) builds `esp32c3`, `esp32`, `esp8266` — and only on pushes to the feature branches, not `main`. There are no unit tests (`test/` is empty); verification means building every affected env and testing on hardware.

`*_migrator` envs build [src/migrator/](src/migrator/) instead of the controller (`build_src_filter`; every other env excludes `migrator/`): a one-off firmware that switches an ESP32/C3/S3 from `default.csv` to `min_spiffs.csv` over the air, see README. It is built against `default.csv` and must stay under its 1.25MB slot. `min_spiffs_table.h` is the framework's `gen_esp32part.py` output - regenerate it rather than editing bytes.

`buildscript_versioning.py` runs pre-build and rewrites [include/version.h](include/version.h) (`SEMVER` env var, else `dev.<timestamp>`). That file is tracked but changes on every build — don't commit it as part of unrelated changes.

## Architecture

- [src/main.cpp](src/main.cpp) — `setup()`/`loop()` only. Creates `CDevice`, `CWifiManager`, `CLEDManager`, wires callbacks, handles the factory-reset smooth-boot timer.
- [src/Configuration.h](src/Configuration.h) / [.cpp](src/Configuration.cpp) — **compile-time feature flags** (`WIFI`, `LED`, `OLED`, `BUTTONS`, `KEYPAD`, `WEB_LOGGING`, `DISABLE_LOGGING`), per-chip defaults (`CONFIG_IDF_TARGET_ESP32C3/S3/ESP32`, `ESP8266`) including `LED_PIN_LIST` (GPIOs offered as LED data pins), the global `configuration_t configuration`, EEPROM load/save/wipe, `CONFIG_getLedBrightness()` (applies the power-save hour window using NTP time), and the LED hardware tables/validation (`LED_TYPE_NAMES`, `LED_COLOR_ORDERS`, `LED_LAYOUT_IDS`, `CONFIG_checkLedHardware()`).
- LED hardware is runtime configuration, set at `/led`: `ledLayout` (`LED_LAYOUT_SINGLE` / `DUAL` / `RING`), `ledPin`, `ledPin2`, `ledType`, `ledColorOrder` (FastLED `EOrder` value), `ledStripSize`, `ledStripSize2`, `ledRingOuterSize`, `ledMirror`. The `LED_PIN`, `LED_STRIP_SIZE`, ... macros are only factory-reset defaults.
- [src/LedManager.cpp](src/LedManager.cpp) — sets up the strips from the configured layout, **registers modes in `registerModes()`**, renders the current mode, handles auto-cycling and the button-driven charging mode. Custom palettes (PayPal, Pride, Christmas, SlavaUkraini) live here.
  - Modes draw into `leds` (`renderSize`); `show()` copies into `hwLeds` (what FastLED sends) only when remapping is needed: a mirrored dual layout, or a non-RGB color order on ESP8266.
  - ESP32 family (`FASTLED_HAS_CHANNELS`): `FastLED.add(fl::ChannelConfig)` takes pin, chipset timing and color order at runtime. ESP8266 has no channel API: one `ClocklessControllerImpl<PIN, TIMING, RGB>` per pin in `LED_PIN_LIST` and timing, with color order applied in software. Adding pins to `LED_PIN_LIST` on ESP8266 costs flash.
- [src/modes/CustomMode.cpp](src/modes/CustomMode.cpp) — user-defined modes from the Mode Configurator (`/modes`). `CUSTOM_EFFECTS` is the effect table (JSON id, label, layout mask, direction options, help) used by the renderer, the page and the JSON import. Each `CCustomMode` draws `configuration.customModes[slot]` and reads it every frame, so saving a mode needs no reboot. All `CUSTOM_MODE_COUNT` slots are registered after the built-in modes, empty or not, so indices stay fixed.
- [src/modes/](src/modes/) — each mode extends `CBaseMode` and implements `draw(CRGB *leds)`, called every loop. `CLEDSegment` + `CRingPaletteMode` split the strip into independently-animated ranges (outer ring size is a constructor argument).
- [src/wifi/WifiManager.cpp](src/wifi/WifiManager.cpp) — STA connect with soft-AP fallback (`<DEVICE_NAME>` + chip id, `192.168.4.1`), ESPAsyncWebServer routes, NTP via POSIX TZ strings, ElegantOTA at `/update`. HTML/CSS are string constants in [HTMLAssets.cpp](src/wifi/HTMLAssets.cpp).
- [src/wifi/WifiManagerMQTT.cpp](src/wifi/WifiManagerMQTT.cpp) — MQTT (PubSubClient) and Home Assistant discovery, as `CWifiManager` members; modeled on [wifi-climate-sensor](https://github.com/jaisor/wifi-climate-sensor). Enabled once `mqttServer` is set on the Device page. Topics under `<mqttTopic>/<device id>`: `availability` (LWT), `state` / `set` (HA JSON-schema light: on/off, brightness, effect = mode name), `config` (same JSON as `POST /config`; saved, reboots on network or LED hardware changes), `json` (telemetry for the diagnostic sensors). Discovery: one `light` plus WiFi signal and IP sensors under `homeassistant/`.
  - The client is only touched from `loop()` (`mqttLoop()`); web handlers set flags like `mqttDiscoveryNeeded`, since PubSubClient isn't safe to call from the async web server's task. State is published whenever power, brightness or mode differ from what was last published, whatever changed them.
  - Connecting blocks while the broker is unreachable, so retries are `MQTT_RECONNECT_MS` apart. HA commands are saved `MQTT_SAVE_DELAY_MS` after the last one to spare flash.
  - The effect list is the modes available on the running layout; republish discovery (`mqttDiscoveryNeeded = true`) whenever mode names or availability can change.
- [src/Device.cpp](src/Device.cpp) — device state machine + optional SSD1306 OLED (72x40 on C3/S3 boards, drawn through a scrolling 128x40 virtual canvas).
- [src/BaseManager.h](src/BaseManager.h) — shared base interface carried over from sibling projects; mostly unused here.

### Web routes
`/` (main UI), `/led` (LED hardware; saves and reboots), `/modes` (Mode Configurator; `POST /modes` JSON saves or deletes one custom mode live and returns the page data), `/wifi`, `/device` (also config export/import), `/style.css`, `/log` (if `WEB_LOGGING`; reading clears the buffer), `POST /factory_reset`, `POST /reboot`, `/mqtt_reconnect` (header link when MQTT is down), `/update` (OTA).
- `GET /config` returns full config JSON (the export file; no WiFi or MQTT password); `POST /config` (JSON body) applies it, **saves to EEPROM and reboots** (the import path, also used by the Device page). A `customModes` array in the body replaces all custom modes.
- `GET /api` returns LED state; `POST /api` applies JSON live **without saving to EEPROM** and ignores LED hardware fields.
- All JSON field handling is in `CWifiManager::updateConfigFromJson()`, which works on a copy and commits only if the whole request is valid; `/led` form posts go through it too. WiFi/NTP fields set `rebootNeeded`.

## Conventions and gotchas

- **`configuration_t` is raw-`EEPROM.put` serialized.** Adding/reordering/resizing fields shifts the layout and corrupts existing devices' settings. The `_loaded == "jaisor"` magic string marks a valid config, so fields must not be inserted before it. Add new fields at the end, after `_loaded`, in a block with its own marker (like the LED hardware block ending in `_ledLoaded == "led"`), give them defaults in `EEPROM_loadConfig()` when the marker is missing, and add range checks.
- Factory reset: 3 boots without surviving `FACTORY_RESET_CLEAR_TIMER_MS` (2s) wipes EEPROM. Counter is EEPROM byte 0; config starts at byte 1.
- Brightness is applied only via `FastLED.show(255 * CONFIG_getLedBrightness())` in `LedManager`. Modes must not call `FastLED.setBrightness()` or `FastLED.show()`.
- Modes must be non-blocking: use the `tMillis` + `configuration.ledDelayMs` pattern, never `delay()`. Use `numLeds` (= runtime `ledStripSize`) for bounds, not `LED_STRIP_SIZE`.
- Adding a mode: create `src/modes/XMode.{h,cpp}` extending `CBaseMode`, include it in `LedManager.cpp`, `addMode(mode, layouts)` it in `registerModes()` with an `LED_LAYOUT_BIT()` mask (or `LED_LAYOUTS_ALL`). Modes are registered on every layout so indices stay stable; ones that don't fit the active layout are hidden in the UI, rejected by the API and skipped when cycling. Mode indices are persisted (`ledMode`, `cycleModesList`), so inserting/reordering changes what users have saved — append at the end, after the custom mode slots.
- Adding a custom-mode effect: append a `CUSTOM_EFFECT_*` value (persisted) and its `CUSTOM_EFFECTS` entry, and draw it in `CCustomMode::draw()`. The page picks it up from the table.
- `htmlTop`'s script posts the first `form[method]` as form data and reloads; pages that handle their own submit (Mode Configurator) use a form without `method`.
- Platform code is guarded with `#if defined(ESP32)` / `#elif defined(ESP8266)`; chip-specific settings use `CONFIG_IDF_TARGET_*`. Any change should compile for all envs, with feature flags both on and off (e.g. `OLED`, `BUTTONS`, `WEB_LOGGING`).
- Logging uses ArduinoLog (`Log.infoln`, `Log.verboseln`, ...). On C3/S3 USB-CDC boards, `while (!Serial)` in `setup()` can block when no host is attached.
- Style: classes prefixed with `C` (`CLEDManager`, `CPaletteMode`), 2-space indent in most files, `#pragma once` headers.
- [.github/copilot-instructions.md](.github/copilot-instructions.md) is partially stale (mentions `/api/led`, `/api/device`, and mode registration in `main.cpp`); trust the code over it.
