#pragma once

#include "BaseMode.h"

class CChristmasRunningMode : public CBaseMode {

private:
    uint16_t position = 0;
    const uint8_t spacing = 6;  // Space between running lights
    CRGB colors[4] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(255, 255, 255), CRGB(255, 215, 0)};  // Red, Green, White, Gold

public:
	CChristmasRunningMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
