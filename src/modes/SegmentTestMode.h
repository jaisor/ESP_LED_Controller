#pragma once

#include "BaseMode.h"

class CSegmentTestMode : public CBaseMode {

public:
    CSegmentTestMode(uint16_t numLeds, String name);
    void draw(CRGB *leds) override;
};
