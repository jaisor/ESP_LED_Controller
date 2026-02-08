#include "SegmentTestMode.h"

CSegmentTestMode::CSegmentTestMode(uint16_t numLeds, String name)
: CBaseMode(numLeds, name) {
}

void CSegmentTestMode::draw(CRGB *leds) {
  uint16_t i = 0;
  for (i = 0; i < 35; i++) {
    leds[i] = CRGB::Red;
  }
  for (i = 35; i < 70; i++) {
    leds[i] = CRGB::Blue;
  }
  for (i = 70; i < 90; i++) {
    leds[i] = CRGB::Green;
  }
  for (i = 90; i < configuration.ledStripSize; i++) {
    leds[i] = CRGB::Yellow;
  }
}
