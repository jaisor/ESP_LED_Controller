#pragma once

#include "BaseMode.h"

struct custom_effect_t {
    const char *id;           // Used in JSON
    const char *label;
    uint8_t layouts;          // LED_LAYOUT_BIT() mask
    const char *directions;   // '|' separated option labels, nullptr if the effect has no direction
    const char *repeatLabel;  // What custom_mode_t::repeat means for this effect, nullptr if unused
    const char *repeatHelp;
    const char *help;
    bool ringSelect;          // On a ring layout, can run on both rings or just one (CUSTOM_MODE_RING_*)
};

// Indexed by CUSTOM_EFFECT_*, entry 0 (CUSTOM_EFFECT_NONE) is a placeholder
extern const custom_effect_t CUSTOM_EFFECTS[CUSTOM_EFFECT_COUNT];

int CUSTOM_effectFromId(const char *id);  // -1 if unknown or CUSTOM_EFFECT_NONE
uint8_t CUSTOM_effectDirectionCount(uint8_t effect);  // Options in its directions list, 1 if it has none
bool CUSTOM_effectSupportsLayout(uint8_t effect, uint8_t layout);

// Draws configuration.customModes[slot], read every frame so edits show without a reboot
class CCustomMode : public CBaseMode {

private:
    const uint8_t slot;
    const uint16_t numLedsOutter;
    const bool ringLayout;
    uint16_t phase = 0;

    // Animation state for the effects that remember the last frame; reset when the effect changes
    struct star_t {
        float pos;        // LED index of the head
        float velocity;   // LEDs per tick, the sign is the direction
        bool active;
    };
    uint8_t stateEffect = CUSTOM_EFFECT_NONE;
    star_t stars[CUSTOM_MODE_MAX_REPEAT];
    uint8_t *heat = nullptr;      // Fire, numLeds cells, only allocated while the effect is fire
    uint16_t accum = 0;           // Sub-step speed accumulator
    uint32_t steps = 0;
    int16_t pos = 0;
    int8_t dir = 1;
    uint8_t colorIndex = 0;
    unsigned long tsNext = 0;     // Lightning: next strike or flash change
    uint8_t flashesLeft = 0;
    bool flashOn = false;
    uint16_t flashStart = 0, flashLength = 0;
    CRGB flashColor;

    CRGB colorAt(const custom_mode_t &m, uint8_t position);
    CRGB gradientAt(const custom_mode_t &m, uint8_t position, bool fromBlack);
    void drawRange(CRGB *leds, const custom_mode_t &m, uint16_t start, uint16_t count, uint8_t shape, bool reverse, uint8_t p);
    uint16_t advance(const custom_mode_t &m, bool tick, uint8_t divider);
    void resetState(uint8_t effect);

    void drawStars(CRGB *leds, const custom_mode_t &m, bool tick);
    void drawFire(CRGB *leds, const custom_mode_t &m, bool tick);
    void burnSegment(CRGB *leds, const custom_mode_t &m, bool tick, uint16_t start, uint16_t length);
    void drawBreathe(CRGB *leds, const custom_mode_t &m);
    void drawChase(CRGB *leds, const custom_mode_t &m, bool tick);
    void drawScanner(CRGB *leds, const custom_mode_t &m, bool tick);
    void drawWipe(CRGB *leds, const custom_mode_t &m, bool tick);
    void drawLightning(CRGB *leds, const custom_mode_t &m);

public:
    CCustomMode(const uint16_t numLeds, const uint16_t numLedsOutter, const uint8_t slot, const bool ringLayout);
    virtual ~CCustomMode();
    virtual void draw(CRGB *leds);
    virtual const String getName();
    virtual bool supportsLayout(uint8_t layout) const;
    virtual int8_t getCustomSlot() const { return slot; }
};
