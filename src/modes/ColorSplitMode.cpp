#include "ColorSplitMode.h"

CColorSplitMode::CColorSplitMode(const uint16_t numLeds, const uint16_t splitAt, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15), splitAt(splitAt) {
}

void CColorSplitMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
      tMillis = millis();
      startIndex = startIndex + 1;
    }

    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
      leds[i] = i < splitAt ? CRGB(0, 87, 184) : CRGB(254, 221, 0);
      ci+=increment;
    }
}