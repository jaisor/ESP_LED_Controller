#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include <FastLED.h>

#include "Configuration.h"
#include "modes/BaseMode.h"

class CLEDManager {

public:
  CLEDManager();
  ~CLEDManager();

  void setup();
  void loop();

  std::vector<CBaseMode*> *getModes() { return &modes; }
  // Layout the strips were set up with at boot; configuration.ledLayout may since hold a pending change
  uint8_t getLayout() const { return layout; }
  void setModeChangeCallback(std::function<void()> callback) { onModeChange = callback; }
  void setChargingStartCallback(std::function<void()> callback) { onChargingStart = callback; }

private:
  void initFastLED();
  void addStrip(uint8_t pin, CRGB *data, uint16_t count);
  void registerModes();
  void addMode(CBaseMode *mode, uint8_t layouts);
  bool isModeAvailable(uint8_t index);
  void handleChargingInput();
  void renderCurrentMode();
  void renderChargingMode();
  void show();
  void updateModeCycling();

  CRGB *leds;          // Modes draw here, renderSize LEDs
  CRGB *hwLeds;        // Registered with FastLED, hwSize LEDs; same buffer as leds unless show() has to remap
  uint16_t renderSize;
  uint16_t hwSize;
  uint8_t layout;
  uint8_t colorOrder;
  bool mirror;
  std::vector<CBaseMode*> modes;
  CBaseMode *chargingMode;

  unsigned long tsCycleMs;
  uint8_t cycleIndex;
  bool isCharging;
  bool wasCharging;

  std::function<void()> onModeChange;
  std::function<void()> onChargingStart;
};
