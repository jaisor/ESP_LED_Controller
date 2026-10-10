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
    uint8_t layouts = LED_LAYOUTS_ALL;

public:
	CBaseMode(const uint16_t numLeds, const String name);
    virtual ~CBaseMode() {}
    virtual void draw(CRGB *leds) {};

    virtual const String getName() { return name; }

    // LED_LAYOUT_BIT() mask of the layouts this mode makes sense on
    void setLayouts(uint8_t mask) { layouts = mask; }
    virtual bool supportsLayout(uint8_t layout) const { return layouts & LED_LAYOUT_BIT(layout); }

    // configuration.customModes slot this mode draws, -1 for built-in modes
    virtual int8_t getCustomSlot() const { return -1; }
};
