#include "ChristmasRunningMode.h"

CChristmasRunningMode::CChristmasRunningMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name) {
}

void CChristmasRunningMode::draw(CRGB *leds) {

    // Run at half speed by doubling the delay
    if (millis() - tMillis > configuration.ledDelayMs * 2) {
        tMillis = millis();
        position = (position + 1) % (numLeds + spacing);
    }

    // Clear all LEDs first
    for (uint16_t i = 0; i < numLeds; i++) {
        leds[i] = CRGB::Black;
    }

    // Draw running lights with Christmas colors and gradual transitions
    for (uint8_t c = 0; c < 4; c++) {
        int16_t ledPos = position - (c * spacing);
        
        // Wrap around for continuous effect
        while (ledPos < 0) {
            ledPos += numLeds + spacing;
        }
        
        if (ledPos < numLeds) {
            // Center LED - full brightness
            leds[ledPos] = colors[c];
            
            // Gradual fade trail (5 LEDs trailing)
            for (int8_t trail = 1; trail <= 5; trail++) {
                int16_t trailPos = ledPos - trail;
                if (trailPos >= 0 && trailPos < numLeds) {
                    CRGB trailColor = colors[c];
                    // Exponential fade: 80%, 60%, 40%, 20%, 10%
                    uint8_t brightness = 255 - (trail * 40);
                    trailColor.nscale8(brightness);
                    leds[trailPos] += trailColor;  // Use += to blend overlapping trails
                }
            }
            
            // Gradual fade forward (2 LEDs forward)
            for (int8_t forward = 1; forward <= 2; forward++) {
                int16_t forwardPos = ledPos + forward;
                if (forwardPos < numLeds) {
                    CRGB forwardColor = colors[c];
                    // Softer forward fade: 60%, 30%
                    uint8_t brightness = 180 - (forward * 90);
                    forwardColor.nscale8(brightness);
                    leds[forwardPos] += forwardColor;  // Use += to blend overlapping
                }
            }
        }
    }
}
