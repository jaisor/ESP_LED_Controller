#include "HalfwayPaletteMode.h"

CHalfwayPaletteMode::CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment)
: CHalfwayPaletteMode(numLeds, name, palette, increment, LINEARBLEND, 15) {
}

CHalfwayPaletteMode::CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType)
: CHalfwayPaletteMode(numLeds, name, palette, increment, blendType, 15) {
}

CHalfwayPaletteMode::CHalfwayPaletteMode(const uint16_t numLeds, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType, const unsigned long delay)
: CBaseMode(numLeds, name), increment(increment), palette(palette), blendType(blendType), delay(delay) {
}

void CHalfwayPaletteMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

    uint16_t center = numLeds / 2;
    
    // Draw from center to both edges
    float ci = startIndex;
    for (uint16_t i = 0; i < center; i++) {
        CRGB color = ColorFromPalette(palette, (uint8_t)ci, 255, blendType);
        
        // Set color on both sides of center
        leds[center - i - 1] = color;  // Left side
        leds[center + i] = color;      // Right side
        
        ci += increment;
    }
    
    // Handle odd number of LEDs - center pixel
    if (numLeds % 2 != 0) {
        leds[center] = ColorFromPalette(palette, (uint8_t)startIndex, 255, blendType);
        leds[numLeds-1] = ColorFromPalette(palette, (uint8_t)ci, 255, blendType);
    }
}
