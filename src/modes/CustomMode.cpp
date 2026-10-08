#include "CustomMode.h"

#define LAYOUTS_STRIP (LED_LAYOUT_BIT(LED_LAYOUT_SINGLE) | LED_LAYOUT_BIT(LED_LAYOUT_DUAL))
#define LAYOUTS_RING LED_LAYOUT_BIT(LED_LAYOUT_RING)

const custom_effect_t CUSTOM_EFFECTS[CUSTOM_EFFECT_COUNT] = {
  { "none", "None", 0, nullptr, false, "" },
  { "fade", "Color fade", LED_LAYOUTS_ALL, nullptr, false,
    "All LEDs show the same color and fade from one palette color to the next." },
  { "scroll", "Edge to edge", LED_LAYOUTS_ALL, "Forward|Backward", true,
    "The palette travels along the LEDs from one end to the other." },
  { "center", "Center to edge", LAYOUTS_STRIP, "Outward|Inward", true,
    "The palette flows from the middle of the LEDs towards both ends, or back to the middle." },
  { "twinkle", "Twinkle", LED_LAYOUTS_ALL, nullptr, false,
    "Random LEDs light up in the palette colors and fade out. Speed sets how often." },
  { "circle", "Circling", LAYOUTS_RING, "Forward|Backward|Rings in opposite directions", true,
    "The palette rotates around each ring." },
  { "halves", "Ring halves", LAYOUTS_RING, "Outward|Inward", true,
    "Each ring is mirrored around its first LED and the palette flows around both sides." },
  { "rings", "Ring to ring", LAYOUTS_RING, "Outer to inner|Inner to outer", false,
    "Each ring shows a single color and the colors pass from one ring to the other." },
};

int CUSTOM_effectFromId(const char *id) {
  for (uint8_t i = CUSTOM_EFFECT_NONE + 1; id && i < CUSTOM_EFFECT_COUNT; i++) {
    if (!strcmp(CUSTOM_EFFECTS[i].id, id)) {
      return i;
    }
  }
  return -1;
}

bool CUSTOM_effectSupportsLayout(uint8_t effect, uint8_t layout) {
  return effect > CUSTOM_EFFECT_NONE && effect < CUSTOM_EFFECT_COUNT
    && (CUSTOM_EFFECTS[effect].layouts & LED_LAYOUT_BIT(layout));
}

enum : uint8_t {
  SHAPE_LINEAR,   // Palette position grows with the LED index
  SHAPE_CENTER,   // ... with the distance from the middle of the range
  SHAPE_HALVES    // ... with the distance from the first LED, going both ways round a ring
};

CCustomMode::CCustomMode(const uint16_t numLeds, const uint16_t numLedsOutter, const uint8_t slot)
: CBaseMode(numLeds, ""), slot(slot), numLedsOutter(numLedsOutter < numLeds ? numLedsOutter : numLeds) {
}

const String CCustomMode::getName() {
  return String(configuration.customModes[slot].name);
}

bool CCustomMode::supportsLayout(uint8_t layout) const {
  return CUSTOM_effectSupportsLayout(configuration.customModes[slot].effect, layout);
}

// Palette is cyclic: position 0-255 runs through all colors and back to the first
CRGB CCustomMode::colorAt(const custom_mode_t &m, uint8_t position) {
  uint16_t scaled = position * m.colorCount;
  uint8_t index = scaled >> 8;
  const uint8_t *a = m.colors[index];
  if (!(m.flags & CUSTOM_MODE_SMOOTH) || m.colorCount < 2) {
    return CRGB(a[0], a[1], a[2]);
  }
  const uint8_t *b = m.colors[(index + 1) % m.colorCount];
  return blend(CRGB(a[0], a[1], a[2]), CRGB(b[0], b[1], b[2]), scaled & 0xFF);
}

void CCustomMode::drawRange(CRGB *leds, const custom_mode_t &m, uint16_t start, uint16_t count, uint8_t shape, bool reverse, uint8_t p) {
  if (count == 0) {
    return;
  }
  for (uint16_t k = 0; k < count; k++) {
    uint32_t d;  // 0..count
    switch (shape) {
      case SHAPE_CENTER: d = abs(2 * (int32_t)k - (int32_t)(count - 1)); break;
      case SHAPE_HALVES: d = 2 * min(k, (uint16_t)(count - k)); break;
      default: d = k;
    }
    uint8_t position = d * 256 * m.repeat / count;
    // Subtracting the phase moves colors towards higher d
    leds[start + k] = colorAt(m, reverse ? position + p : position - p);
  }
}

void CCustomMode::draw(CRGB *leds) {
  const custom_mode_t &m = configuration.customModes[slot];
  if (m.effect == CUSTOM_EFFECT_NONE) {
    return;
  }

  bool tick = millis() - tMillis > configuration.ledDelayMs;
  if (tick) {
    tMillis = millis();
    phase += m.speed;
  }
  // Speed 1 takes ~1000 frames for a full palette cycle, speed 10 about 100
  uint8_t p = phase >> 2;
  uint16_t inner = numLeds - numLedsOutter;

  switch (m.effect) {
    case CUSTOM_EFFECT_FADE:
      fill_solid(leds, numLeds, colorAt(m, p));
      break;
    case CUSTOM_EFFECT_SCROLL:
      drawRange(leds, m, 0, numLeds, SHAPE_LINEAR, m.direction == 1, p);
      break;
    case CUSTOM_EFFECT_CENTER:
      drawRange(leds, m, 0, numLeds, SHAPE_CENTER, m.direction == 1, p);
      break;
    case CUSTOM_EFFECT_TWINKLE:
      // Keeps the previous frame, so only change it on a tick
      if (tick) {
        fadeToBlackBy(leds, numLeds, 4 + m.speed * 2);
        if (random8() < 30 + m.speed * 22) {
          for (uint16_t i = 0; i <= numLeds / 64; i++) {
            leds[random16(numLeds)] = colorAt(m, random8());
          }
        }
      }
      break;
    case CUSTOM_EFFECT_CIRCLE:
      drawRange(leds, m, 0, numLedsOutter, SHAPE_LINEAR, m.direction == 1, p);
      drawRange(leds, m, numLedsOutter, inner, SHAPE_LINEAR, m.direction != 0, p);
      break;
    case CUSTOM_EFFECT_HALVES:
      drawRange(leds, m, 0, numLedsOutter, SHAPE_HALVES, m.direction == 1, p);
      drawRange(leds, m, numLedsOutter, inner, SHAPE_HALVES, m.direction == 1, p);
      break;
    case CUSTOM_EFFECT_RINGS: {
      // The trailing ring shows the color the leading ring had one palette step earlier
      CRGB leading = colorAt(m, p);
      CRGB trailing = colorAt(m, p - (uint8_t)(256 / m.colorCount));
      fill_solid(leds, numLedsOutter, m.direction == 1 ? trailing : leading);
      fill_solid(leds + numLedsOutter, inner, m.direction == 1 ? leading : trailing);
    } break;
  }
}
