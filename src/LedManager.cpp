#include "LedManager.h"

#include <ArduinoLog.h>

#include "modes/HoneyOrangeMode.h"
#include "modes/PaletteMode.h"
#include "modes/HalfwayPaletteMode.h"
#include "modes/RingPaletteMode.h"
#include "modes/ColorSplitMode.h"
#include "modes/SlavaUkrainiRingMode.h"
#include "modes/ChristmasRunningMode.h"
#include "modes/ChristmasRunningModeReverse.h"
#include "modes/ChargingMode.h"
#include "modes/WhiteLightMode.h"
#include "modes/PixelSeparatorMode.h"
#include "modes/CustomMode.h"

#if FASTLED_HAS_CHANNELS && FASTLED_RMT5
  #include "platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.h"
#endif

const TProgmemRGBPalette16 PayPal_p FL_PROGMEM =
{
    0x253B80,   // PayPal Dark Blue
    0x253B80,   // PayPal Dark Blue
    0x169BD7,   // PayPal Light Blue
    0x169BD7,   // PayPal Light Blue

    0x222D65,   // PayPal Navy
    0x222D65,   // PayPal Navy
    0xFFFFFF,   // White
    0xFFFFFF,   // White

    0xFFFFFF,   // White
    0xFFFFFF,   // White
    0x222D65,   // PayPal Navy
    0x222D65,   // PayPal Navy

    0x169BD7,   // PayPal Light Blue
    0x169BD7,   // PayPal Light Blue
    0x253B80,   // PayPal Dark Blue
    0x253B80,   // PayPal Dark Blue
};

#define S1C1 0xFFD700
#define S1C2 0x665700
#define S2C1 0x0057B8
#define S2C2 0x003066

const TProgmemRGBPalette16 SlavaUkraini_p FL_PROGMEM =
{
    S1C2,
    S1C2,
    S1C1,
    S1C1,

    S2C1,
    S2C1,
    S2C2,
    S2C2,

    S2C2,
    S2C2,
    S2C1,
    S2C1,

    S1C1,
    S1C1,
    S1C2,
    S1C2,
};

const TProgmemRGBPalette16 Pride_p FL_PROGMEM =
{
    0xFF0018,   // Vivid Red
    0xFF0018,   // Vivid Red
    0xFFA52C,   // Deep Saffron
    0xFFA52C,   // Deep Saffron

    0xFFFF41,   // Maximum Yellow
    0xFFFF41,   // Maximum Yellow
    0x008018,   // Ao
    0x008018,   // Ao

    0x0000F9,   // Blue
    0x0000F9,   // Blue
    0x86007D,   // Philippine Violet
    0x86007D,   // Philippine Violet

    0x86007D,   // Philippine Violet
    0x86007D,   // Philippine Violet
    0xFF0018,   // Vivid Red
    0xFF0018,   // Vivid Red
};

const TProgmemRGBPalette16 Christmas_p FL_PROGMEM =
{
    0x00FF00,   // Bright Green
    0x00CC00,   // Green
    0x009900,   // Dark Green
    0x228B22,   // Forest Green

    0x32CD32,   // Lime Green
    0x4CBB17,   // Kelly Green
    0xFF6347,   // Tomato Red
    0xFF4500,   // Orange Red

    0xFF0000,   // Red
    0xDC143C,   // Crimson
    0xB22222,   // Fire Brick
    0x8B0000,   // Dark Red

    0xA52A2A,   // Brown
    0xFF0000,   // Red
    0x00AA00,   // Medium Green
    0x00FF00,   // Bright Green
};

#if FASTLED_HAS_CHANNELS
// ESP32 family: FastLED channels take pin, timing and color order at runtime
static fl::ClocklessChipset clocklessChipset(uint8_t type, uint8_t pin) {
  switch (type) {
    case 2:  return fl::makeClockless<fl::TIMING_WS2813>(pin);
    case 3:  return fl::makeClockless<fl::TIMING_WS2815>(pin);
    case 4:  return fl::makeClockless<fl::TIMING_SK6812>(pin);
    case 5:  // TM1809
    case 6:  return fl::makeClockless<fl::TIMING_TM1809_800KHZ>(pin); // TM1804
    case 7:  return fl::makeClockless<fl::TIMING_TM1803_400KHZ>(pin);
    case 8:  return fl::makeClockless<fl::TIMING_UCS1903_400KHZ>(pin);
    case 9:  return fl::makeClockless<fl::TIMING_UCS1904_800KHZ>(pin);
    case 10: return fl::makeClockless<fl::TIMING_GS1903>(pin);
    case 11: return fl::makeClockless<fl::TIMING_PL9823>(pin);
    case 13: return fl::makeClockless<fl::TIMING_WS2811_800KHZ_LEGACY>(pin);
    default: return fl::makeClockless<fl::TIMING_WS2812_800KHZ>(pin); // WS2812B, WS2812, WS2852
  }
}
#else
// ESP8266: pin is a template parameter, so there is one controller class per timing and LED_PIN_LIST pin.
// Only the ones in use are allocated. They all send RGB; show() puts the channels in the configured
// order while copying into hwLeds.
template <typename TIMING>
static CLEDController* clocklessController(uint8_t pin) {
  switch (pin) {
    #define LED_PIN_CASE(p) case p: return new fl::ClocklessControllerImpl<p, TIMING, RGB>();
    LED_PIN_LIST(LED_PIN_CASE)
    #undef LED_PIN_CASE
  }
  return nullptr;
}

static CLEDController* clocklessController(uint8_t type, uint8_t pin) {
  switch (type) {
    case 2:  return clocklessController<fl::TIMING_WS2813>(pin);
    case 3:  return clocklessController<fl::TIMING_WS2815>(pin);
    case 4:  return clocklessController<fl::TIMING_SK6812>(pin);
    case 5:  // TM1809
    case 6:  return clocklessController<fl::TIMING_TM1809_800KHZ>(pin); // TM1804
    case 7:  return clocklessController<fl::TIMING_TM1803_400KHZ>(pin);
    case 8:  return clocklessController<fl::TIMING_UCS1903_400KHZ>(pin);
    case 9:  return clocklessController<fl::TIMING_UCS1904_800KHZ>(pin);
    case 10: return clocklessController<fl::TIMING_GS1903>(pin);
    case 11: return clocklessController<fl::TIMING_PL9823>(pin);
    case 13: return clocklessController<fl::TIMING_WS2811_800KHZ_LEGACY>(pin);
    default: return clocklessController<fl::TIMING_WS2812_800KHZ>(pin); // WS2812B, WS2812, WS2852
  }
}

// Byte n on the wire is the channel named by octal digit n of the EOrder, same as FastLED's own reordering
static inline CRGB toWireOrder(const CRGB &c, uint8_t order) {
  return CRGB(c.raw[(order >> 6) & 0x3], c.raw[(order >> 3) & 0x3], c.raw[order & 0x3]);
}
#endif

CLEDManager::CLEDManager()
: leds(nullptr), hwLeds(nullptr), renderSize(0), hwSize(0), layout(LED_LAYOUT_SINGLE), colorOrder(static_cast<uint8_t>(RGB)), mirror(false),
  chargingMode(nullptr), tsCycleMs(0), cycleIndex(0), isCharging(false), wasCharging(false) {}

CLEDManager::~CLEDManager() {
  for (auto *mode : modes) {
    delete mode;
  }
  modes.clear();

  delete chargingMode;
  chargingMode = nullptr;

  if (hwLeds != leds) {
    delete[] hwLeds;
  }
  hwLeds = nullptr;
  delete[] leds;
  leds = nullptr;
}

void CLEDManager::setup() {
  #ifdef BUTTONS
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  #endif

  layout = configuration.ledLayout;
  colorOrder = configuration.ledColorOrder;
  mirror = layout == LED_LAYOUT_DUAL && configuration.ledMirror;
  hwSize = CONFIG_getLedCount(configuration);
  // A mirrored second strip repeats the first, so modes only draw the first strip's length
  renderSize = mirror ? configuration.ledStripSize : hwSize;

  bool remap = mirror;
  #if !FASTLED_HAS_CHANNELS
    remap = remap || colorOrder != static_cast<uint8_t>(RGB);
  #endif
  leds = new CRGB[renderSize];
  hwLeds = remap ? new CRGB[hwSize] : leds;

  Log.infoln("LED layout '%s': %d LEDs, %s, color order %s", LED_LAYOUT_IDS[layout], hwSize,
    LED_TYPE_NAMES[configuration.ledType], CONFIG_ledColorOrderName(colorOrder));
  initFastLED();

  FastLED.setBrightness(255);
  CONFIG_getLedBrightness(true);

  registerModes();
  chargingMode = new CChargingMode(renderSize, "Charging");

  tsCycleMs = millis();
}

void CLEDManager::loop() {

  if (modes.empty()) {
    configuration.ledMode = 0;
    return;
  }

  if (!isModeAvailable(configuration.ledMode)) {
    // Saved mode is out of range or doesn't fit this layout - fall back to the first one that does
    configuration.ledMode = 0;
    while (configuration.ledMode < modes.size() - 1 && !isModeAvailable(configuration.ledMode)) {
      configuration.ledMode++;
    }
  }
  handleChargingInput();

  if (isCharging) {
    renderChargingMode();
  } else {
    renderCurrentMode();
    updateModeCycling();
  }
}

void CLEDManager::initFastLED() {
  #if FASTLED_HAS_CHANNELS && FASTLED_RMT5
    // RMT streams each frame through a small ping-pong buffer that an interrupt refills. With FastLED's
    // default 2-3 memory blocks the refill is due every ~80us, and WiFi interrupts can hold it off for
    // 50-120us - the transmission then runs on stale symbols and the rest of the frame glitches. Give
    // each strip an equal share of all TX memory instead (ESP32: 8 blocks, ~320us for one strip).
    size_t blocks = SOC_RMT_TX_CANDIDATES_PER_GROUP / (layout == LED_LAYOUT_DUAL ? 2 : 1);
    fl::RmtMemoryManager::instance().setMemoryBlockStrategy(blocks, blocks);
    Log.infoln("RMT buffer: %d memory blocks per strip", blocks);
  #endif
  addStrip(configuration.ledPin, hwLeds, configuration.ledStripSize);
  if (layout == LED_LAYOUT_DUAL) {
    addStrip(configuration.ledPin2, hwLeds + configuration.ledStripSize, hwSize - configuration.ledStripSize);
  }
}

void CLEDManager::addStrip(uint8_t pin, CRGB *data, uint16_t count) {
  Log.infoln("Adding %d LEDs on GPIO %d", count, pin);
#if FASTLED_HAS_CHANNELS
  fl::ChannelOptions options;
  options.mCorrection = TypicalLEDStrip;
  // Left on AUTO, classic ESP32 picks the I2S engine, which needs ~400 bytes of contiguous DMA RAM per
  // byte of LED data (150KB for 128 LEDs) - its allocation fails and no frame is ever sent. RMT streams
  // through small buffers and is what addLeds<>() used on all three chips.
  options.mBus = fl::Bus::RMT;
  fl::ChannelConfig config(clocklessChipset(configuration.ledType, pin), fl::span<CRGB>(data, count), (EOrder)colorOrder, options);
  if (FastLED.add(config) == nullptr) {
    Log.errorln("Unable to drive LEDs on GPIO %d", pin);
  }
#else
  CLEDController *controller = clocklessController(configuration.ledType, pin);
  if (!controller) {
    Log.errorln("GPIO %d can't drive LEDs", pin);
    return;
  }
  // Correction scales wire bytes, so it gets the same reordering as the pixels
  FastLED.addLeds(controller, data, count).setCorrection(toWireOrder(CRGB(TypicalLEDStrip), colorOrder));
#endif
}

void CLEDManager::addMode(CBaseMode *mode, uint8_t layouts) {
  mode->setLayouts(layouts);
  modes.push_back(mode);
}

bool CLEDManager::isModeAvailable(uint8_t index) {
  return index < modes.size() && modes[index]->supportsLayout(layout);
}

void CLEDManager::registerModes() {
  // Indices are persisted in ledMode and cycleModesList - append new modes at the end
  const uint16_t n = renderSize;
  addMode(new CPaletteMode(n, "Party Colors", PartyColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  //
  addMode(new CHalfwayPaletteMode(n, "Halfway Rainbow", RainbowColors_p, 255.0 / ((float)n / 2.0)), LED_LAYOUTS_ALL);
  addMode(new CHalfwayPaletteMode(n, "Halfway Cloud", CloudColors_p, 255.0 / ((float)n / 2.0)), LED_LAYOUTS_ALL);
  addMode(new CHalfwayPaletteMode(n, "Halfway Party", PartyColors_p, 255.0 / ((float)n / 2.0)), LED_LAYOUTS_ALL);
  //
  addMode(new CPaletteMode(n, "Heat Colors", HeatColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  addMode(new CPaletteMode(n, "Rainbow Colors", RainbowColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  addMode(new CPaletteMode(n, "Cloud Colors", CloudColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  addMode(new CPaletteMode(n, "Forest Colors", ForestColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  addMode(new CPaletteMode(n, "Ocean Colors", OceanColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  addMode(new CPaletteMode(n, "Lava Colors", LavaColors_p, 255.0 / (float)n), LED_LAYOUTS_ALL);
  //
  addMode(new CWhiteLightMode(n, "White Light"), LED_LAYOUTS_ALL);

  // Ring light modes animate the outer and inner ring separately. They are created on every layout
  // so indices stay put, but only drawn on a ring - elsewhere they just get a harmless split point.
  const uint8_t ring = LED_LAYOUT_BIT(LED_LAYOUT_RING);
  const uint16_t outer = layout == LED_LAYOUT_RING ? configuration.ledRingOuterSize : n / 2;
  const float ringIncrement = 255.0 / (float)n * 2.0;
  addMode(new CSlavaUkrainiRingMode(n, outer, "Slava Ukraini"), ring);
  addMode(new CRingPaletteMode(n, outer, "Slava Ukraini 2", SlavaUkraini_p, ringIncrement), ring);
  addMode(new CColorSplitMode(n, outer, "Dual Ring"), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Party Colors", PartyColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Heat Colors", HeatColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Rainbow Colors", RainbowColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Cloud Colors", CloudColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Forest Colors", ForestColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Ocean Colors", OceanColors_p, ringIncrement), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Lava Colors", LavaColors_p, ringIncrement), ring);
  addMode(new CHoneyOrangeMode(n, outer, "Honey Amber"), ring);
  addMode(new CRingPaletteMode(n, outer, "Ring Pride", Pride_p, ringIncrement), ring);

  // Custom mode slots (Mode Configurator). All of them are registered so their indices don't move;
  // empty slots and effects that don't fit the layout report themselves unavailable.
  // New built-in modes go after these.
  for (uint8_t slot = 0; slot < CUSTOM_MODE_COUNT; slot++) {
    modes.push_back(new CCustomMode(n, outer, slot));
  }
}

void CLEDManager::handleChargingInput() {
  #if defined(BUTTONS) && defined(LED)
    isCharging = (digitalRead(BUTTON_2_PIN) == LOW);

    if (isCharging && !wasCharging) {
      Log.infoln("Charging started");
      if (onChargingStart) {
        onChargingStart();
      }
    }

    wasCharging = isCharging;
  #else
    isCharging = false;
    wasCharging = false;
  #endif
}

void CLEDManager::renderCurrentMode() {
  if (modes.empty()) {
    return;
  }

  modes[configuration.ledMode]->draw(leds);
  show();
}

void CLEDManager::renderChargingMode() {
  if (!chargingMode) {
    return;
  }

  chargingMode->draw(leds);
  show();
}

void CLEDManager::show() {
  if (hwLeds != leds) {
    for (uint16_t i = 0; i < renderSize; i++) {
      #if FASTLED_HAS_CHANNELS
        hwLeds[i] = leds[i];
      #else
        hwLeds[i] = toWireOrder(leds[i], colorOrder);
      #endif
    }
    if (mirror) {
      ::memcpy(hwLeds + renderSize, hwLeds, renderSize * sizeof(CRGB));
    }
  }
  FastLED.show(255 * CONFIG_getLedBrightness());
}

void CLEDManager::updateModeCycling() {
  if (configuration.ledCycleModeMs == 0 || modes.empty()) {
    return;
  }

  if (millis() - tsCycleMs <= configuration.ledCycleModeMs) {
    return;
  }

  tsCycleMs = millis();

  // Step to the next mode that fits this layout, from the custom list if it has any such mode
  bool found = false;
  for (uint8_t i = 0; i < configuration.cycleModesCount && !found; i++) {
    cycleIndex = (cycleIndex + 1) % configuration.cycleModesCount;
    if (isModeAvailable(configuration.cycleModesList[cycleIndex])) {
      configuration.ledMode = configuration.cycleModesList[cycleIndex];
      found = true;
    }
  }
  for (size_t i = 0; i < modes.size() && !found; i++) {
    configuration.ledMode = (configuration.ledMode + 1) % modes.size();
    found = isModeAvailable(configuration.ledMode);
  }

  if (onModeChange) {
    onModeChange();
  }
  Log.verboseln("Switching modes to '%s'", modes[configuration.ledMode]->getName().c_str());
}
