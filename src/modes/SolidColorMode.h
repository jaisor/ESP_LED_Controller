#pragma once

#include "BaseMode.h"
#include <U8g2_for_Adafruit_GFX.h>

class GFXcanvas1; // forward declaration

class CSolidColorMode : public CBaseMode {

private:
    CRGB color;
    uint8_t modeIndex;
    GFXcanvas1* canvas;
    U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

public:
    CSolidColorMode(const uint16_t numLeds, const String name, const CRGB color, const uint8_t modeIndex);
    ~CSolidColorMode();
    virtual void draw(CRGB *leds);
    virtual CRGB getColor() const override { return color; }
};
