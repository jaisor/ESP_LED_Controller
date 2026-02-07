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
  void setModeChangeCallback(std::function<void()> callback) { onModeChange = callback; }
  void setChargingStartCallback(std::function<void()> callback) { onChargingStart = callback; }

private:
  void initFastLED();
  void registerModes();
  void handleModeBounds();
  void handleChargingInput();
  void renderCurrentMode();
  void renderChargingMode();
  void updateModeCycling();
  
  CRGB *leds;
  std::vector<CBaseMode*> modes;
  CBaseMode *chargingMode;

  unsigned long tsCycleMs;
  uint8_t cycleIndex;
  bool isCharging;
  bool wasCharging;

  std::function<void()> onModeChange;
  std::function<void()> onChargingStart;
};
