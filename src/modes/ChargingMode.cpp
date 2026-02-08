#include "ChargingMode.h"

CChargingMode::CChargingMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name) {
    startTime = millis();
}

void CChargingMode::draw(CRGB *leds) {
    // Calculate progress as a triangle wave: 0→1 over 10s, then 1→0 over 10s
    unsigned long elapsed = (millis() - startTime) % CYCLE_DURATION_MS;
    unsigned long halfCycle = CYCLE_DURATION_MS / 2;
    float blend;
    if (elapsed < halfCycle) {
        blend = (float)elapsed / (float)halfCycle;           // 0.0 → 1.0 (red-yellow → green)
    } else {
        blend = 1.0f - (float)(elapsed - halfCycle) / (float)halfCycle; // 1.0 → 0.0 (green → red-yellow)
    }

    for (uint16_t i = 0; i < numLeds; i++) {
        // Base: red-to-yellow gradient that shifts over time
        float shiftSpeed = (float)elapsed / 1000.0f; // one full shift per second
        float pos = fmod((float)i / (float)(numLeds - 1) + shiftSpeed, 1.0f);
        CRGB redYellow = CRGB(
            255,                          // R stays 255 across red→yellow
            (uint8_t)(pos * 255),         // G ramps 0→255 (red→yellow)
            0                             // B stays 0
        );

        // Target: solid green
        CRGB green = CRGB::Green;

        // Blend between red-yellow gradient and green
        leds[i] = redYellow.lerp8(green, (uint8_t)(blend * 255));
    }

    // Green pixel bouncing between index 70 and 90 over the full 20s cycle
    if (numLeds > 70) {
        float cycleProgress = (float)elapsed / (float)CYCLE_DURATION_MS; // 0.0 → 1.0
        // Triangle wave: 0→1→0 over one cycle for bounce effect
        float bounce = (cycleProgress < 0.5f)
            ? (cycleProgress * 2.0f)
            : (2.0f - cycleProgress * 2.0f);
        uint16_t greenStart = 70;
        uint16_t greenEnd = min((uint16_t)90, (uint16_t)(numLeds - 1));
        uint16_t greenPos = greenStart + (uint16_t)(bounce * (greenEnd - greenStart));
        if (greenPos < numLeds) {
            leds[greenPos] = CRGB::Green;
        }
    }
}
