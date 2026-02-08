#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include <FastLED.h>

#include "Configuration.h"
#include "modes/BaseMode.h"

// LED strip segment boundary indices
#define SEG_BOTTOM_RING_SIZE  35
#define SEG_WALL_RING_END     69
#define SEG_VERTICAL_ARM_END  90
// top_arm runs from SEG_VERTICAL_ARM_END to ledStripSize

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
  void handleChargingInput();
  void renderCurrentMode();
  void renderChargingMode();
  void updateModeCycling();
  void copyVirtualToHardware();
  
  CRGB *leds;           // Hardware LED array (ledStripSize)
  CRGB *virtualLeds;    // Virtual LED array (ledStripSize - SEG_BOTTOM_RING_SIZE)
  uint16_t virtualSize; // Size of the virtual array

  std::vector<CBaseMode*> modes;
  CBaseMode *chargingMode;

  unsigned long tsCycleMs;
  uint8_t cycleIndex;
  bool isCharging;
  bool wasCharging;

  std::function<void()> onModeChange;
  std::function<void()> onChargingStart;
};
