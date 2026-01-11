# ESP32 LED Controller - AI Agent Instructions

## Project Overview
ESP32/ESP8266-based WiFi-controlled LED strip controller using PlatformIO and FastLED library. Supports multiple LED animation modes, web-based configuration, OTA firmware updates, and dual ring configurations.

## Architecture

### Core Components
- **[main.cpp](../src/main.cpp)**: Entry point, initializes FastLED, WiFi, and LED modes. Manages mode cycling and rendering loop.
- **[Configuration.h](../src/Configuration.h)**: Feature flags (`#define WIFI`, `#define LED`, `#define RING_LIGHT`) and default settings. EEPROM-backed persistent configuration via `configuration_t` struct.
- **[WifiManager](../src/wifi/)**: Async web server (ESPAsyncWebServer) serving control UI, REST API endpoints (`/api/led`, `/api/device`, `/api/config`), and OTA updates at `/update`.
- **[Mode System](../src/modes/)**: Inheritance-based pattern. All modes extend `CBaseMode` and implement `draw(CRGB *leds)` for frame rendering.

### Data Flow
1. Configuration loaded from EEPROM → `configuration_t` global struct
2. LED modes registered in `std::vector<CBaseMode*> modes` during `setup()`
3. Main loop: `modes[configuration.ledMode]->draw(leds)` → `FastLED.show()`
4. Web UI/API modifies `configuration` → saves to EEPROM → changes take effect immediately

## Hardware Variants

### Configuration Switching
Use **conditional compilation** to switch between hardware profiles:
- **Ring Light**: `#define RING_LIGHT` in [Configuration.h](../src/Configuration.h#L10) for dual-ring setup (141 outer + 126 inner LEDs)
- **LED Strip**: Comment out `RING_LIGHT` for single strip (default 61 LEDs)
- Alternative configs available: [Configuration.h.ringlight](../src/Configuration.h.ringlight), [Configuration.h.ledstrip](../src/Configuration.h.ledstrip)

### Pin Assignments
- ESP32: GPIO12 (default), configurable via `LED_PIN` in Configuration.h
- ESP8266: D3 (GPIO0) alternative, but ESP32 is recommended for stability

## Development Workflows

### Building & Flashing
```bash
# Build for ESP32 (default environment)
pio run -e esp32

# Build for ESP8266
pio run -e esp8266

# Upload firmware via USB
pio run -e esp32 -t upload

# Monitor serial output
pio device monitor -b 115200
```

### OTA Updates
Once deployed, upload `firmware.bin` to `http://<device_ip>/update` (ElegantOTA library handles this).

### Versioning
[buildscript_versioning.py](../buildscript_versioning.py) runs pre-build to generate [include/version.h](../include/version.h). Set `SEMVER` env var for releases, otherwise uses `dev.{timestamp}`.

## Code Patterns

### Creating New LED Modes
1. Extend `CBaseMode` in [src/modes/](../src/modes/)
2. Constructor: `CYourMode(uint16_t numLeds, String name) : CBaseMode(numLeds, name)`
3. Implement `void draw(CRGB *leds)` - called every frame
4. Register in [main.cpp](../src/main.cpp#L156): `modes.push_back(new CYourMode(...))`
5. Access global config via `configuration.ledDelayMs`, `configuration.ledStripSize`, etc.

**Pattern Examples**:
- **Palette-based**: See [PaletteMode.cpp](../src/modes/PaletteMode.cpp) - uses `ColorFromPalette()` with `startIndex` animation
- **Ring modes**: See [RingPaletteMode.cpp](../src/modes/RingPaletteMode.cpp) - uses `CLEDSegment` to split rings independently
- **Static modes**: See [WhiteLightMode.cpp](../src/modes/WhiteLightMode.cpp) - simple `fill_solid()` calls

### EEPROM Persistence
- **Never modify** `configuration_t` layout without migration logic (breaks existing EEPROM data)
- Call `EEPROM_saveConfig()` after modifying `configuration` struct
- Magic string `"jaisor"` in `configuration._loaded` validates EEPROM integrity

### Factory Reset Mechanism
Detects 3+ reboots within 3 seconds (`FACTORY_RESET_CLEAR_TIMER_MS`). Counter stored at `EEPROM_FACTORY_RESET` byte. Cleared after smooth boot in [main.cpp](../src/main.cpp#L221).

## Platform-Specific Notes

### ESP32 vs ESP8266 Differences
- **Preprocessor guards**: Use `#ifdef ESP32` / `#elif ESP8266` for platform-specific code
- **WiFi headers**: `<WiFi.h>` (ESP32) vs `<ESP8266WiFi.h>` (ESP8266)
- **Async libraries**: `<AsyncTCP.h>` (ESP32) vs `<ESPAsyncTCP.h>` (ESP8266)
- **Stability**: ESP8266 has known WiFi stability issues with long strips (prefer ESP32)

### LED Library (FastLED)
- **Initialization**: Dynamic LED type selection via `configuration.ledType` (0-13) in [main.cpp](../src/main.cpp#L143)
- **Brightness**: Global brightness via `FastLED.show(255 * CONFIG_getLedBrightness())` - never modify `FastLED.setBrightness()` directly
- **Color order**: `LED_COLOR_ORDER` macro (default GRB) set in Configuration.h

## Web API Endpoints

### REST API Structure
- **GET/POST /api/config**: Full configuration JSON
- **GET/POST /api/led**: LED-specific settings (mode, brightness, strip size)
- **GET /api/device**: Device info (uptime, version, IP)
- **POST /factoryreset**, **POST /reboot**: Device control

### JSON Schema
Configuration uses ArduinoJson v7. See `updateConfigFromJson()` in [WifiManager.cpp](../src/wifi/WifiManager.cpp) for supported fields.

## Common Pitfalls

1. **Don't call `FastLED.setBrightness()` in modes** - brightness is managed globally in main loop
2. **Avoid blocking delays** in `draw()` - use `millis()` timers like `tMillis` pattern in BaseMode
3. **Check `configuration.ledStripSize`** before array access - user-configurable at runtime
4. **Test both `RING_LIGHT` defined/undefined** - mode registration differs (see conditional blocks in main.cpp setup)
5. **`yield()` in main loop** is critical for ESP8266 watchdog stability

## Dependencies
All managed via [platformio.ini](../platformio.ini) `lib_deps`:
- FastLED 3.10.3+ (LED control)
- ESPAsyncWebServer (async HTTP)
- ArduinoJson 7.3.0+ (config serialization)
- ElegantOTA 3.1.6+ (OTA updates)

No manual installation required - PlatformIO handles automatically.
