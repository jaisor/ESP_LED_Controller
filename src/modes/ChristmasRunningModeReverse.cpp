#include "ChristmasRunningModeReverse.h"

CChristmasRunningModeReverse::CChristmasRunningModeReverse(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name) {
}

void CChristmasRunningModeReverse::draw(CRGB *leds) {

    // Run at half speed by doubling the delay
    if (millis() - tMillis > configuration.ledDelayMs * 2) {
        tMillis = millis();
        // Reverse direction: decrement instead of increment
        if (position == 0) {
            position = numLeds + spacing - 1;
        } else {
            position--;
        }
        cycleCount = (cycleCount + 1) % (5 * (numLeds + spacing));
    }

    // Calculate fade brightness (0-255) over 5 cycles
    float progress = (float)cycleCount / (float)(5 * (numLeds + spacing));
    uint8_t fadeBrightness;
    if (progress < 0.5) {
        // Fade in during first half (0% to 100%)
        fadeBrightness = (uint8_t)(progress * 2.0 * 255.0);
    } else {
        // Fade out during second half (100% to 0%)
        fadeBrightness = (uint8_t)((1.0 - (progress - 0.5) * 2.0) * 255.0);
    }

    // Clear all LEDs first
    for (uint16_t i = 0; i < numLeds; i++) {
        leds[i] = CRGB::Black;
    }

    // Draw running lights with Red, White, Blue colors and gradual transitions
    for (uint8_t c = 0; c < 3; c++) {
        int16_t ledPos = position - (c * spacing);
        
        // Wrap around for continuous effect
        while (ledPos < 0) {
            ledPos += numLeds + spacing;
        }
        
        if (ledPos < numLeds) {
            // Center LED - full brightness with fade applied
            leds[ledPos] = colors[c];
            leds[ledPos].nscale8(fadeBrightness);
            
            // Gradual fade trail (5 LEDs trailing) - now in reverse direction
            for (int8_t trail = 1; trail <= 5; trail++) {
                int16_t trailPos = ledPos + trail;  // Changed from minus to plus for reverse
                if (trailPos >= 0 && trailPos < numLeds) {
                    CRGB trailColor = colors[c];
                    // Exponential fade: 80%, 60%, 40%, 20%, 10%
                    uint8_t brightness = 255 - (trail * 40);
                    trailColor.nscale8(brightness);
                    trailColor.nscale8(fadeBrightness);  // Apply cycle fade
                    leds[trailPos] += trailColor;  // Use += to blend overlapping trails
                }
            }
            
            // Gradual fade forward (2 LEDs forward) - now in reverse direction
            for (int8_t forward = 1; forward <= 2; forward++) {
                int16_t forwardPos = ledPos - forward;  // Changed from plus to minus for reverse
                if (forwardPos >= 0 && forwardPos < numLeds) {
                    CRGB forwardColor = colors[c];
                    // Softer forward fade: 60%, 30%
                    uint8_t brightness = 180 - (forward * 90);
                    forwardColor.nscale8(brightness);
                    forwardColor.nscale8(fadeBrightness);  // Apply cycle fade
                    leds[forwardPos] += forwardColor;  // Use += to blend overlapping
                }
            }
        }
    }
}
