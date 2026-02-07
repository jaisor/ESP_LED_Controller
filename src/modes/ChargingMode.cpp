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
    
    // Calculate the position of the running green pixel
    uint16_t pixelPosition = (uint16_t)(progress * numLeds);
    
    // Calculate the gradient shift amount (oscillates red to yellow)
    float gradientShift = sin(progress * 2.0 * PI) * 0.5 + 0.5; // 0.0 to 1.0
    
    // Draw background gradient and running pixel
    for (uint16_t i = 0; i < numLeds; i++) {
        // Create red-yellow gradient background
        float ledPosition = (float)i / (float)numLeds;
        
        // Gradient shifts from red to yellow based on position and time
        uint8_t hue = HUE_RED + (uint8_t)((ledPosition + gradientShift) * (HUE_YELLOW - HUE_RED)) % (HUE_YELLOW - HUE_RED + 1);
        
        // Set background color
        leds[i] = CHSV(hue, 255, 200);
        
        // Draw the green running pixel
        if (i == pixelPosition) {
            leds[i] = CRGB::Green;
        }
    }
}
