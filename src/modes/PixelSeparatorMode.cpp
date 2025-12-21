#include "PixelSeparatorMode.h"

CPixelSeparatorMode::CPixelSeparatorMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15) {
}

void CPixelSeparatorMode::draw(CRGB *leds) {

  if (millis() - tMillis > configuration.ledDelayMs) {
    tMillis = millis();
    startIndex = startIndex + 1;
  }

  float ci = startIndex;
  for( uint16_t i = 0; i < numLeds; i++) {
    uint8_t c = (255 * i) / numLeds;
    if (i % 2 == 0) {
      leds[i] = CRGB(c, 0, 0);
    } else if (i % 3 == 0) {
      leds[i] = CRGB(0, c, 0);
    } else {
      leds[i] = CRGB(0, 0, c);
    }
    ci+=increment;
  }
}
