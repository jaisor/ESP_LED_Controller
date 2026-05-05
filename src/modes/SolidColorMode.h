#pragma once

#include "BaseMode.h"

class CSolidColorMode : public CBaseMode {

private:
    CRGB color;

public:
    CSolidColorMode(const uint16_t numLeds, const String name, const CRGB color);
    virtual void draw(CRGB *leds);
    virtual CRGB getColor() const override { return color; }
};
