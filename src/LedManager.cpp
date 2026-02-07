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

CLEDManager::CLEDManager()
: leds(nullptr), chargingMode(nullptr), tsCycleMs(0), cycleIndex(0), isCharging(false), wasCharging(false) {}

CLEDManager::~CLEDManager() {
  for (auto *mode : modes) {
    delete mode;
  }
  modes.clear();

  delete chargingMode;
  chargingMode = nullptr;

  delete[] leds;
  leds = nullptr;
}

void CLEDManager::setup() {
  #ifdef BUTTONS
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  #endif

  leds = new CRGB[configuration.ledStripSize];
  initFastLED();

  Log.infoln("LED Type configured: %d", configuration.ledType);
  FastLED.setBrightness(255);
  CONFIG_getLedBrightness(true);

  registerModes();
  chargingMode = new CChargingMode(configuration.ledStripSize, "Charging");

  tsCycleMs = millis();
}

void CLEDManager::loop() {
  handleModeBounds();
  handleChargingInput();

  if (isCharging) {
    renderChargingMode();
  } else {
    renderCurrentMode();
    updateModeCycling();
  }
}

void CLEDManager::initFastLED() {
  switch(configuration.ledType) {
    case 0:  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 1:  FastLED.addLeds<WS2812, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 2:  FastLED.addLeds<WS2813, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 3:  FastLED.addLeds<WS2815, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 4:  FastLED.addLeds<SK6812, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 5:  FastLED.addLeds<TM1809, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 6:  FastLED.addLeds<TM1804, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 7:  FastLED.addLeds<TM1803, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 8:  FastLED.addLeds<UCS1903, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 9:  FastLED.addLeds<UCS1904, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 10: FastLED.addLeds<GS1903, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 11: FastLED.addLeds<PL9823, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 12: FastLED.addLeds<WS2852, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    case 13: FastLED.addLeds<WS2811, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
    default: FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection(TypicalLEDStrip); break;
  }
}

void CLEDManager::registerModes() {
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Party Colors", PartyColors_p, 255.0 / (float)configuration.ledStripSize));
  //
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Halfway Rainbow", RainbowColors_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Halfway Cloud", CloudColors_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Halfway Party", PartyColors_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  //
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Heat Colors", HeatColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Rainbow Colors", RainbowColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Cloud Colors", CloudColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Forest Colors", ForestColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Ocean Colors", OceanColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Lava Colors", LavaColors_p, 255.0 / (float)configuration.ledStripSize));
  //
  modes.push_back(new CWhiteLightMode(configuration.ledStripSize, "White Light"));
}

void CLEDManager::handleModeBounds() {
  if (modes.empty()) {
    configuration.ledMode = 0;
    return;
  }

  if (configuration.ledMode > modes.size() - 1) {
    configuration.ledMode = 0;
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
  FastLED.show(255 * CONFIG_getLedBrightness());
}

void CLEDManager::renderChargingMode() {
  if (!chargingMode) {
    return;
  }

  chargingMode->draw(leds);
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

  if (configuration.cycleModesCount > 0) {
    cycleIndex = (cycleIndex + 1) % configuration.cycleModesCount;
    uint8_t nextMode = configuration.cycleModesList[cycleIndex];
    if (nextMode < modes.size()) {
      configuration.ledMode = nextMode;
    } else {
      cycleIndex = 0;
      configuration.ledMode = configuration.cycleModesList[0];
    }
  } else {
    configuration.ledMode++;
    if (configuration.ledMode > modes.size() - 1) {
      configuration.ledMode = 0;
    }
  }

  if (onModeChange) {
    onModeChange();
  }
  Log.verboseln("Switching modes to '%s'", modes[configuration.ledMode]->getName().c_str());
}
