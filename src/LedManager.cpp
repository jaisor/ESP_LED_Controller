#include "LedManager.h"

#include <ArduinoLog.h>

#include "modes/ChargingMode.h"
#include "modes/SolidColorMode.h"



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

  if (modes.empty()) {
    configuration.ledMode = 0;
    return;
  }

  if (configuration.ledMode > modes.size() - 1) {
    configuration.ledMode = 0;
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
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Green",   CRGB(0,   255, 0)));    // 0
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Red",     CRGB(255, 0,   0)));    // 1
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Blue",    CRGB(0,   0,   255)));  // 2
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Yellow",  CRGB(255, 255, 0)));    // 3
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Cyan",    CRGB(0,   255, 255)));  // 4
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Magenta", CRGB(255, 0,   255)));  // 5
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Orange",  CRGB(255, 128, 0)));    // 6
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Purple",  CRGB(148, 0,   211)));  // 7
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "White",   CRGB(255, 255, 255)));  // 8
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Pink",    CRGB(255, 105, 180)));  // 9
  modes.push_back(new CSolidColorMode(configuration.ledStripSize, "Teal",    CRGB(0,   128, 128)));  // 10
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
