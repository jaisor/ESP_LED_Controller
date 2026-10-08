#pragma once

#include "BaseMode.h"

struct custom_effect_t {
    const char *id;           // Used in JSON
    const char *label;
    uint8_t layouts;          // LED_LAYOUT_BIT() mask
    const char *directions;   // '|' separated option labels, nullptr if the effect has no direction
    bool usesRepeat;
    const char *help;
};

// Indexed by CUSTOM_EFFECT_*, entry 0 (CUSTOM_EFFECT_NONE) is a placeholder
extern const custom_effect_t CUSTOM_EFFECTS[CUSTOM_EFFECT_COUNT];

int CUSTOM_effectFromId(const char *id);  // -1 if unknown or CUSTOM_EFFECT_NONE
bool CUSTOM_effectSupportsLayout(uint8_t effect, uint8_t layout);

// Draws configuration.customModes[slot], read every frame so edits show without a reboot
class CCustomMode : public CBaseMode {

private:
    const uint8_t slot;
    const uint16_t numLedsOutter;
    uint16_t phase = 0;

    CRGB colorAt(const custom_mode_t &m, uint8_t position);
    void drawRange(CRGB *leds, const custom_mode_t &m, uint16_t start, uint16_t count, uint8_t shape, bool reverse, uint8_t p);

public:
    CCustomMode(const uint16_t numLeds, const uint16_t numLedsOutter, const uint8_t slot);
    virtual void draw(CRGB *leds);
    virtual const String getName();
    virtual bool supportsLayout(uint8_t layout) const;
    virtual int8_t getCustomSlot() const { return slot; }
};
