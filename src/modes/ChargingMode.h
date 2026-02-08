#pragma once

#include "BaseMode.h"

class CChargingMode : public CBaseMode {

private:
    unsigned long startTime = 0;
    static const unsigned long CYCLE_DURATION_MS = 20000; // 20 seconds full cycle (10s forward + 10s back)

public:
	CChargingMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
