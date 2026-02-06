#include "ChargingMode.h"

CChargingMode::CChargingMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name) {
    startTime = millis();
}

void CChargingMode::draw(CRGB *leds) {
    // Calculate elapsed time and progress (0.0 to 1.0)
    unsigned long elapsed = millis() - startTime;
    if (elapsed > CHARGE_DURATION_MS) {
        // Reset and restart after 15 seconds
        startTime = millis();
        elapsed = 0;
    }
    
    float progress = (float)elapsed / (float)CHARGE_DURATION_MS;
    
    // Calculate how many LEDs should be lit based on progress
    uint16_t litLeds = (uint16_t)(progress * numLeds);
    
    // Fill LEDs progressively
    for (uint16_t i = 0; i < numLeds; i++) {
        if (i < litLeds) {
            // LED is lit - calculate its color based on position
            // Transition from red (0) -> yellow (64) -> green (96)
            float ledProgress = (float)i / (float)numLeds;
            
            uint8_t hue;
            if (ledProgress < 0.5) {
                // First half: red to yellow
                hue = HUE_RED + (uint8_t)((ledProgress * 2.0) * (HUE_YELLOW - HUE_RED));
            } else {
                // Second half: yellow to green
                hue = HUE_YELLOW + (uint8_t)(((ledProgress - 0.5) * 2.0) * (HUE_GREEN - HUE_YELLOW));
            }
            
            leds[numLeds - i] = CHSV(hue, 255, 255);
        } else {
            // LED is off
            leds[numLeds - i] = CRGB::Red;
        }
    }
}
