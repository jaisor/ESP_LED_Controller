#pragma once

#include "BaseMode.h"

class CChristmasRunningModeReverse : public CBaseMode {

private:
    uint16_t position = 0;
    uint16_t cycleCount = 0;
    const uint8_t spacing = 6;  // Space between running lights
    const uint16_t totalCycleLength = 5 * (numLeds + spacing);  // 5 full cycles
    CRGB colors[3] = {CRGB(255, 255, 0), CRGB(0, 255, 0), CRGB(128, 0, 128)};  // Yellow, Green, Purple

public:
	CChristmasRunningModeReverse(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
