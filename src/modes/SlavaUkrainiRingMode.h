#pragma once

#include "BaseMode.h"

class CSlavaUkrainiRingMode : public CBaseMode {

private:
    uint8_t startIndex = 0;
    const float increment;
    const TBlendType blendType;
    const unsigned long delay;
    std::vector<CLEDSegment> segments;

public:
	CSlavaUkrainiRingMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
