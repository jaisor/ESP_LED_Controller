#pragma once

#include "BaseMode.h"

class CHalfwayPaletteMode : public CBaseMode {

private:
    uint8_t startIndex = 0;
    const float increment;
    const TProgmemRGBPalette16& palette;
    const TBlendType blendType;
    const unsigned long delay;

public:
	CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment);
    CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType);
    CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType, const unsigned long delay);
    virtual void draw(CRGB *leds);
};
