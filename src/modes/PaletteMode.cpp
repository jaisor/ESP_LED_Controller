#include "PaletteMode.h"

CPaletteMode::CPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment)
: CPaletteMode(numLeds, name, palette, increment, LINEARBLEND, 15) {
}

CPaletteMode::CPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType)
: CPaletteMode(numLeds, name, palette, increment, blendType, 15) {
}

CPaletteMode::CPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType, const unsigned long delay)
: CBaseMode(numLeds, name), increment(increment), palette(palette), blendType(blendType), delay(delay) {
}

void CPaletteMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
        leds[i] = ColorFromPalette( palette, (uint8_t)ci, 255, blendType);
        ci+=increment;
    }
}