#include "WhiteLightMode.h"

CWhiteLightMode::CWhiteLightMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15) {
}

void CWhiteLightMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
        leds[i] = CRGB(255, 255, 255);
        ci+=increment;
    }
}