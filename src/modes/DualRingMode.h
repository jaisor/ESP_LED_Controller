#pragma once

#include "BaseMode.h"

class CDualRingMode : public CBaseMode {

private:
    uint8_t startIndex = 0;
    const float increment;
    //const TProgmemRGBPalette16& palette1, palette2;
    const TBlendType blendType;
    const unsigned long delay;

public:
	CDualRingMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
