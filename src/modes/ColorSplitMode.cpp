#include "ColorSplitMode.h"

CColorSplitMode::CColorSplitMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15) {
}

void CColorSplitMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
      tMillis = millis();
      startIndex = startIndex + 1;
    }

    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
    #ifdef RING_LIGHT
      leds[i] = i < OUTTER_RING_SIZE ? CRGB(0, 87, 184) : CRGB(254, 221, 0);
    #else
      leds[i] = i < numLeds / 2 ? CRGB(0, 87, 184) : CRGB(254, 221, 0);
    #endif
      ci+=increment;
    }
}