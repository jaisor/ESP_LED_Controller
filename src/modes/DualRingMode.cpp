#include "DualRingMode.h"

#define RING_1_SIZE 141

/*
extern const TProgmemRGBPalette16 Ring1_p FL_PROGMEM =
{
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,

    HONEY_ORANGE,
    HONEY_ORANGE,
    HONEY_ORANGE,
    HONEY_ORANGE,

    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    
    HONEY_YELLOW,
    HONEY_YELLOW,
    HONEY_YELLOW,
    HONEY_YELLOW,
};

extern const TProgmemRGBPalette16 Ring2_p FL_PROGMEM =
{
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,
    HONEY_DARK_ORANGE,

    HONEY_ORANGE,
    HONEY_ORANGE,
    HONEY_ORANGE,
    HONEY_ORANGE,

    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    HONEY_LIGHT_ORANGE,
    
    HONEY_YELLOW,
    HONEY_YELLOW,
    HONEY_YELLOW,
    HONEY_YELLOW,
};

*/

CDualRingMode::CDualRingMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15) {
}

void CDualRingMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
        leds[i] = i < RING_1_SIZE ? CRGB(0, 87, 184) : CRGB(254, 221, 0);
        ci+=increment;
    }
}