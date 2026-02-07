#pragma once

#include "BaseMode.h"

class CChargingMode : public CBaseMode {

private:
    unsigned long startTime = 0;
    const unsigned long CHARGE_DURATION_MS = 15000; // 15 seconds
    const uint8_t HUE_RED = 0;
    const uint8_t HUE_YELLOW = 64;
    const uint8_t HUE_GREEN = 96;

public:
	CChargingMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
