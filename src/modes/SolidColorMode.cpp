#include "SolidColorMode.h"

CSolidColorMode::CSolidColorMode(const uint16_t numLeds, const String name, const CRGB color)
: CBaseMode(numLeds, name), color(color) {
}

void CSolidColorMode::draw(CRGB *leds) {
    fill_solid(leds, numLeds, color);
}
