#include "RingPaletteMode.h"

CRingPaletteMode::CRingPaletteMode(const uint16_t numLeds, const uint16_t numLedsOutter, const String name, const TProgmemRGBPalette16& palette, const float increment)
: CRingPaletteMode(numLeds, numLedsOutter, name, palette, increment, LINEARBLEND, 15) {
}

CRingPaletteMode::CRingPaletteMode(const uint16_t numLeds, const uint16_t numLedsOutter, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType)
: CRingPaletteMode(numLeds, numLedsOutter, name, palette, increment, blendType, 15) {
}

CRingPaletteMode::CRingPaletteMode(const uint16_t numLeds, const uint16_t numLedsOutter, const String name, const TProgmemRGBPalette16& palette, const float increment, const TBlendType blendType, const unsigned long delay)
: CBaseMode(numLeds, name), numLedsOutter(numLedsOutter), increment(increment), palette(palette), blendType(blendType), delay(delay) {

    segments.push_back(CLEDSegment(numLedsOutter * 0.5, 0, palette));
    segments.push_back(CLEDSegment(numLedsOutter * 0.5, numLedsOutter, palette));
    segments.push_back(CLEDSegment(numLedsOutter + (numLeds - numLedsOutter) * 0.5, numLedsOutter, palette));
    segments.push_back(CLEDSegment(numLedsOutter + (numLeds - numLedsOutter) * 0.5, numLeds, palette));
    
}

void CRingPaletteMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

    for(CLEDSegment s : segments) {

        float ci = startIndex;
        if (s.start <= s.end) {
            for( uint16_t i = s.end-1; i >= s.start; i--) {
                leds[i] = ColorFromPalette(s.palette, (uint8_t)ci, 255, blendType);
                ci+=increment;
            }
        } else {
            for( uint16_t i = s.end; i < s.start; i++) {
                leds[i] = ColorFromPalette(s.palette, (uint8_t)ci, 255, blendType);
                ci+=increment;
            }
        }
        
    }
}