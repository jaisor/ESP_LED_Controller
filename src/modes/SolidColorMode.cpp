#include "SolidColorMode.h"
#include <Adafruit_GFX.h>

static const uint8_t  MATRIX_SIZE    = 8;
static const uint16_t MATRIX2_OFFSET = 64;
static const uint16_t MATRIX2_END    = MATRIX2_OFFSET + MATRIX_SIZE * MATRIX_SIZE; // 128

CSolidColorMode::CSolidColorMode(const uint16_t numLeds, const String name, const CRGB color, const uint8_t modeIndex)
: CBaseMode(numLeds, name), color(color), modeIndex(modeIndex), canvas(nullptr) {
    if (numLeds >= MATRIX2_END) {
        canvas = new GFXcanvas1(MATRIX_SIZE, MATRIX_SIZE);
        canvas->fillScreen(0);
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", modeIndex);
        u8g2_for_adafruit_gfx.begin(*canvas);
        u8g2_for_adafruit_gfx.setFont(u8g2_font_pressstart2p_8u);
        u8g2_for_adafruit_gfx.setForegroundColor(1);
        u8g2_for_adafruit_gfx.setBackgroundColor(0);
        // Center using actual glyph width; must be called after setFont
        int16_t textW = u8g2_for_adafruit_gfx.getUTF8Width(buf);
        int16_t cx = (MATRIX_SIZE - textW) / 2;
        if (cx < 0) cx = 0;
        u8g2_for_adafruit_gfx.drawStr(cx+1, MATRIX_SIZE+1, buf);
    }
}

CSolidColorMode::~CSolidColorMode() {
    delete canvas;
}

void CSolidColorMode::draw(CRGB *leds) {

    if (canvas && numLeds >= MATRIX2_END) {
        fill_solid(leds, MATRIX2_OFFSET, color);
        for (uint8_t x = 0; x < MATRIX_SIZE; x++) {
            for (uint8_t y = 0; y < MATRIX_SIZE; y++) {
                leds[MATRIX2_OFFSET + (y * MATRIX_SIZE) + x] = canvas->getPixel(x, y) ? color : CRGB::Black;
            }
        }
    } else {
        fill_solid(leds, numLeds, color);
    }
}
