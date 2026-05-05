#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "Configuration.h"

class CLEDSegment {

public:
    const uint16_t start, end;
    const TProgmemRGBPalette16& palette;

	CLEDSegment(const uint16_t start, const uint16_t end, const TProgmemRGBPalette16& palette)
    : start(start), end(end), palette(palette) {};
};

class CBaseMode {

protected:
    unsigned long tMillis;
    const uint16_t numLeds;
    const String name;

public:
	CBaseMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds) {};
    virtual CRGB getColor() const { return CRGB(0, 0, 0); }

    const String getName() { return name; }
};
