#include "CustomMode.h"

#include <new>

#define LAYOUTS_STRIP (LED_LAYOUT_BIT(LED_LAYOUT_SINGLE) | LED_LAYOUT_BIT(LED_LAYOUT_DUAL))
#define LAYOUTS_RING LED_LAYOUT_BIT(LED_LAYOUT_RING)

#define REPEATS "Palette repeats", "How many times the colors repeat across the LEDs"

const custom_effect_t CUSTOM_EFFECTS[CUSTOM_EFFECT_COUNT] = {
  { "none", "None", 0, nullptr, nullptr, nullptr, "" },
  { "fade", "Color fade", LED_LAYOUTS_ALL, nullptr, nullptr, nullptr,
    "All LEDs show the same color and fade from one palette color to the next." },
  { "scroll", "Edge to edge", LED_LAYOUTS_ALL, "Forward|Backward", REPEATS,
    "The palette travels along the LEDs from one end to the other." },
  { "center", "Center to edge", LAYOUTS_STRIP, "Outward|Inward", REPEATS,
    "The palette flows from the middle of the LEDs towards both ends, or back to the middle." },
  { "twinkle", "Twinkle", LED_LAYOUTS_ALL, nullptr, nullptr, nullptr,
    "Random LEDs light up in the palette colors and fade out. Speed sets how often." },
  { "circle", "Circling", LAYOUTS_RING, "Forward|Backward|Rings in opposite directions", REPEATS,
    "The palette rotates around each ring." },
  { "halves", "Ring halves", LAYOUTS_RING, "Outward|Inward", REPEATS,
    "Each ring is mirrored around its first LED and the palette flows around both sides." },
  { "rings", "Ring to ring", LAYOUTS_RING, "Outer to inner|Inner to outer", nullptr, nullptr,
    "Each ring shows a single color and the colors pass from one ring to the other." },
  { "stars", "Falling stars", LED_LAYOUTS_ALL, "Toward the nearest end|Forward|Backward",
    "Stars at once", "How many stars can be in flight together",
    "Stars light up at random LEDs and race toward an end. The head takes the first color and the trail "
    "fades through the others, e.g. white then blue." },
  { "fire", "Flickering flame", LED_LAYOUTS_ALL, "From the start|From the end|From both ends|From the center",
    "Flame height", "Higher lets the heat travel further before it cools",
    "A fire simulation. List the colors from the coolest to the hottest part of the flame, e.g. red, orange, "
    "yellow; the coldest parts fade to black. Speed sets how often new sparks flare up. On a ring light "
    "each ring burns on its own.", true },
  { "breathe", "Breathing", LED_LAYOUTS_ALL, nullptr, nullptr, nullptr,
    "All LEDs slowly glow up and fade out, taking the next color with each breath." },
  { "chase", "Theater chase", LED_LAYOUTS_ALL, "Forward|Backward",
    "Spacing", "LEDs from one lit LED to the next",
    "Evenly spaced lit LEDs march along like marquee lights, each taking the next color in turn." },
  { "scanner", "Scanner", LED_LAYOUTS_ALL, nullptr,
    "Eye width", "LEDs in the bright eye",
    "A bright eye sweeps from end to end with a fading tail, switching to the next color at each end." },
  { "wipe", "Color wipe", LED_LAYOUTS_ALL, "Forward|Backward|From the center", nullptr, nullptr,
    "Each color in turn fills the LEDs, pushing out the previous one." },
  { "lightning", "Lightning", LED_LAYOUTS_ALL, nullptr,
    "Flashes per strike", "Each strike is a burst of up to this many flashes",
    "Bursts of flashes on random stretches of the LEDs, in the palette colors, with dark pauses in between. "
    "Speed sets how often it strikes." },
};

int CUSTOM_effectFromId(const char *id) {
  for (uint8_t i = CUSTOM_EFFECT_NONE + 1; id && i < CUSTOM_EFFECT_COUNT; i++) {
    if (!strcmp(CUSTOM_EFFECTS[i].id, id)) {
      return i;
    }
  }
  return -1;
}

uint8_t CUSTOM_effectDirectionCount(uint8_t effect) {
  uint8_t count = 1;
  if (effect < CUSTOM_EFFECT_COUNT) {
    for (const char *d = CUSTOM_EFFECTS[effect].directions; d && *d; d++) {
      if (*d == '|') count++;
    }
  }
  return count;
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

CCustomMode::CCustomMode(const uint16_t numLeds, const uint16_t numLedsOutter, const uint8_t slot, const bool ringLayout)
: CBaseMode(numLeds, ""), slot(slot), numLedsOutter(numLedsOutter < numLeds ? numLedsOutter : numLeds), ringLayout(ringLayout) {
  memset(stars, 0, sizeof(stars));
}

CCustomMode::~CCustomMode() {
  delete[] heat;
}

void CCustomMode::resetState(uint8_t effect) {
  stateEffect = effect;
  memset(stars, 0, sizeof(stars));
  if (effect != CUSTOM_EFFECT_FIRE) {
    delete[] heat;
    heat = nullptr;
  } else if (heat) {
    memset(heat, 0, numLeds);
  }
  accum = 0;
  steps = 0;
  pos = 0;
  dir = 1;
  colorIndex = 0;
  tsNext = millis();
  flashesLeft = 0;
  flashOn = false;
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

// Palette as a line: position 0 is the first color (or black), 255 the last color
CRGB CCustomMode::gradientAt(const custom_mode_t &m, uint8_t position, bool fromBlack) {
  uint8_t stops = m.colorCount + (fromBlack ? 1 : 0);
  auto stop = [&](uint8_t i) -> CRGB {
    if (fromBlack && i == 0) return CRGB::Black;
    const uint8_t *c = m.colors[i - (fromBlack ? 1 : 0)];
    return CRGB(c[0], c[1], c[2]);
  };
  if (stops < 2) {
    return stop(0);
  }
  uint16_t scaled = position * (stops - 1);
  uint8_t index = scaled >> 8;
  uint8_t frac = scaled & 0xFF;
  if (!(m.flags & CUSTOM_MODE_SMOOTH)) {
    return stop(frac < 128 ? index : index + 1);
  }
  return blend(stop(index), stop(index + 1), frac);
}

// Whole steps to take this tick at the mode's speed; higher dividers move slower
uint16_t CCustomMode::advance(const custom_mode_t &m, bool tick, uint8_t divider) {
  if (!tick) {
    return 0;
  }
  accum += m.speed;
  uint16_t n = accum / divider;
  accum %= divider;
  return n;
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

  if (m.effect != stateEffect) {
    resetState(m.effect);
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
    case CUSTOM_EFFECT_STARS: drawStars(leds, m, tick); break;
    case CUSTOM_EFFECT_FIRE: drawFire(leds, m, tick); break;
    case CUSTOM_EFFECT_BREATHE: drawBreathe(leds, m); break;
    case CUSTOM_EFFECT_CHASE: drawChase(leds, m, tick); break;
    case CUSTOM_EFFECT_SCANNER: drawScanner(leds, m, tick); break;
    case CUSTOM_EFFECT_WIPE: drawWipe(leds, m, tick); break;
    case CUSTOM_EFFECT_LIGHTNING: drawLightning(leds, m); break;
  }
}

void CCustomMode::drawStars(CRGB *leds, const custom_mode_t &m, bool tick) {
  const uint16_t trail = constrain(numLeds / 8, 3, 40);
  if (tick) {
    for (uint8_t i = 0; i < CUSTOM_MODE_MAX_REPEAT; i++) {
      star_t &s = stars[i];
      if (i >= m.repeat) {
        s.active = false;
      } else if (s.active) {
        s.pos += s.velocity;
        // Done once the whole trail has left the LEDs
        if (s.pos < -(float)trail || s.pos > (float)(numLeds + trail)) {
          s.active = false;
        }
      } else if (random8() < 6 + m.speed * 3) {
        s.pos = random16(numLeds);
        int8_t towards = m.direction == 1 ? 1 : m.direction == 2 ? -1 : (s.pos < numLeds / 2.0f ? -1 : 1);
        // 0.18 (speed 1) to 0.9 (speed 10) LEDs per tick, +-30% per star so they don't move in lockstep
        s.velocity = towards * (0.1f + m.speed * 0.08f) * (0.7f + random8() / 425.0f);
        s.active = true;
      }
    }
  }

  fill_solid(leds, numLeds, CRGB::Black);
  for (uint8_t i = 0; i < m.repeat && i < CUSTOM_MODE_MAX_REPEAT; i++) {
    const star_t &s = stars[i];
    if (!s.active) continue;
    int32_t head = lroundf(s.pos);
    int8_t back = s.velocity > 0 ? -1 : 1;
    for (uint16_t k = 0; k < trail; k++) {
      int32_t index = head + back * (int32_t)k;
      if (index < 0 || index >= numLeds) continue;
      uint8_t t = k * 255 / trail;
      CRGB c = gradientAt(m, t, false);
      c.nscale8(255 - t);
      leds[index] += c;  // Saturating, so crossing stars add up
    }
  }
}

// Fire2012 (Mark Kriegsman): cool every cell, drift heat away from the base, ignite sparks at the base.
// On a ring layout each ring burns on its own, and CUSTOM_MODE_RING_* can leave one of them dark.
void CCustomMode::drawFire(CRGB *leds, const custom_mode_t &m, bool tick) {
  if (!heat) {
    heat = new (std::nothrow) uint8_t[numLeds]();
    if (!heat) {
      fill_solid(leds, numLeds, CRGB::Black);
      return;
    }
  }
  if (!ringLayout) {
    burnSegment(leds, m, tick, 0, numLeds);
    return;
  }

  const uint16_t inner = numLeds - numLedsOutter;
  if (m.flags & CUSTOM_MODE_RING_INNER) {
    fill_solid(leds, numLedsOutter, CRGB::Black);
  } else {
    burnSegment(leds, m, tick, 0, numLedsOutter);
  }
  if (m.flags & CUSTOM_MODE_RING_OUTER) {
    fill_solid(leds + numLedsOutter, inner, CRGB::Black);
  } else {
    burnSegment(leds, m, tick, numLedsOutter, inner);
  }
}

// One independent fire on leds[start, start + length), using the same cells of heat as its state
void CCustomMode::burnSegment(CRGB *leds, const custom_mode_t &m, bool tick, uint16_t start, uint16_t length) {
  if (length == 0) {
    return;
  }
  uint8_t *cells = heat + start;
  // From both ends or from the center, two half-length flames mirror each other
  const bool mirrored = m.direction >= 2;
  const uint16_t burning = mirrored ? (length + 1) / 2 : length;

  if (tick) {
    // Flame length goes roughly with 1/cooling, so this spreads heights 1..10 evenly over ~20-95% of the LEDs
    const uint16_t cooling = 6500 / (14 + 8 * m.repeat);  // 295..69
    const uint8_t sparking = 40 + m.speed * 16;       // Speed 1..10 -> spark chance 56..200 of 255
    const uint8_t coolMax = min<uint16_t>(255, cooling * 10 / burning + 2);
    for (uint16_t i = 0; i < burning; i++) {
      cells[i] = qsub8(cells[i], random8(coolMax));
    }
    for (uint16_t k = burning - 1; k >= 2 && k < burning; k--) {
      cells[k] = (cells[k - 1] + cells[k - 2] + cells[k - 2]) / 3;
    }
    if (random8() < sparking) {
      uint8_t y = random8(min<uint16_t>(7, burning));
      cells[y] = qadd8(cells[y], random8(160, 255));
    }
  }

  for (uint16_t i = 0; i < length; i++) {
    uint16_t cell;
    switch (m.direction) {
      case 1: cell = length - 1 - i; break;                                        // Base at the end
      case 2: cell = min<uint16_t>(i, length - 1 - i); break;                      // Bases at both ends
      case 3: cell = abs(2 * (int32_t)i - (int32_t)(length - 1)) / 2; break;       // Base in the middle
      default: cell = i;                                                           // Base at the start
    }
    leds[start + i] = gradientAt(m, cells[cell], true);
  }
}

void CCustomMode::drawBreathe(CRGB *leds, const custom_mode_t &m) {
  // One breath per 1024 phase units: ~10s at speed 1, ~1s at speed 10. The color changes while dark.
  const uint8_t *c = m.colors[(phase >> 10) % m.colorCount];
  CRGB color(c[0], c[1], c[2]);
  color.nscale8(quadwave8((phase & 1023) >> 2));
  fill_solid(leds, numLeds, color);
}

void CCustomMode::drawChase(CRGB *leds, const custom_mode_t &m, bool tick) {
  const uint8_t spacing = m.repeat + 1;
  const int32_t period = (int32_t)spacing * m.colorCount;  // Lights and their colors repeat after this
  steps = (steps + advance(m, tick, 24)) % period;
  for (uint16_t i = 0; i < numLeds; i++) {
    int32_t p = m.direction == 1 ? (int32_t)i + (int32_t)steps : (int32_t)i - (int32_t)steps;
    p = ((p % period) + period) % period;
    if (p % spacing) {
      leds[i] = CRGB::Black;
    } else {
      const uint8_t *c = m.colors[p / spacing];
      leds[i] = CRGB(c[0], c[1], c[2]);
    }
  }
}

void CCustomMode::drawScanner(CRGB *leds, const custom_mode_t &m, bool tick) {
  if (!tick) {
    return;  // The tail lives in the previous frame
  }
  const uint16_t width = min<uint16_t>(m.repeat, numLeds);
  const int16_t span = numLeds - width;
  fadeToBlackBy(leds, numLeds, 48);
  for (uint16_t n = advance(m, tick, 6); n > 0 && span > 0; n--) {
    pos += dir;
    if (pos >= span) {
      pos = span;
      dir = -1;
      colorIndex++;
    } else if (pos <= 0) {
      pos = 0;
      dir = 1;
      colorIndex++;
    }
  }
  const uint8_t *c = m.colors[colorIndex % m.colorCount];
  fill_solid(leds + pos, width, CRGB(c[0], c[1], c[2]));
}

void CCustomMode::drawWipe(CRGB *leds, const custom_mode_t &m, bool tick) {
  const bool fromCenter = m.direction == 2;
  const uint16_t length = fromCenter ? (numLeds + 1) / 2 : numLeds;
  steps += advance(m, tick, 6);
  while (steps > length) {  // A full fill is length + 1 steps, then the next color starts over
    steps -= length + 1;
    colorIndex = (colorIndex + 1) % m.colorCount;
  }
  const uint8_t current = colorIndex % m.colorCount;
  const uint8_t *n = m.colors[current];
  const uint8_t *o = m.colors[(current + m.colorCount - 1) % m.colorCount];
  for (uint16_t i = 0; i < numLeds; i++) {
    uint16_t d = fromCenter ? abs(2 * (int32_t)i - (int32_t)(numLeds - 1)) / 2
               : m.direction == 1 ? numLeds - 1 - i
               : i;
    leds[i] = d < steps ? CRGB(n[0], n[1], n[2]) : CRGB(o[0], o[1], o[2]);
  }
}

// Timed in milliseconds rather than frames, since flashes are tens of milliseconds long
void CCustomMode::drawLightning(CRGB *leds, const custom_mode_t &m) {
  const unsigned long now = millis();
  if ((long)(now - tsNext) >= 0) {
    if (flashOn) {
      flashOn = false;
      if (flashesLeft) {
        tsNext = now + random16(40, 160);  // Dark gap within a strike
      } else {
        // Between strikes: 2.7-13s at speed 1, 0.3-1.3s at speed 10
        tsNext = now + (uint32_t)random16(800, 4000) * (CUSTOM_MODE_MAX_SPEED + 1 - m.speed) / 3;
      }
    } else {
      if (!flashesLeft) {
        flashesLeft = 1 + random8(m.repeat);
      }
      flashesLeft--;
      flashOn = true;
      uint16_t shortest = max<uint16_t>(1, numLeds / 6);
      flashLength = random16(shortest, numLeds + 1);
      flashStart = random16(numLeds - flashLength + 1);
      flashColor = colorAt(m, random8());
      flashColor.nscale8(random8(150, 255));
      tsNext = now + random16(20, 70);
    }
  }
  fill_solid(leds, numLeds, CRGB::Black);
  if (flashOn) {
    fill_solid(leds + flashStart, flashLength, flashColor);
  }
}
