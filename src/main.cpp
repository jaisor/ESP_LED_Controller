#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>


#ifndef ESP8266
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <FastLED.h>
#include <vector>

#include "modes/HoneyOrangeMode.h"
#include "modes/PaletteMode.h"

#define BRIGHTNESS        255  // 0-255
#define CHANGE_MODE_SEC   60

//#define LED_PIN_BOARD     2
#define LED_PIN_STRIP     2
#define NUM_LEDS          71

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

std::vector<CBaseMode*> modes;

void setup() {
  delay( 1000 ); // power-up safety delay

  Serial.begin(115200);
  while(!Serial && !Serial.available()){}
  randomSeed(analogRead(0));

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  FastLED.addLeds<LED_TYPE, LED_PIN_STRIP, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

#ifdef LED_PIN_BOARD
  pinMode(LED_PIN_BOARD, OUTPUT);
  digitalWrite(LED_PIN_BOARD, HIGH);
#endif

  modes.push_back(new CPaletteMode(NUM_LEDS, PartyColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, HeatColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, RainbowColors_p, 255 / NUM_LEDS ));
  modes.push_back(new CPaletteMode(NUM_LEDS, CloudColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, ForestColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, OceanColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, HeatColors_p, 255 / NUM_LEDS));
  modes.push_back(new CHoneyOrangeMode(NUM_LEDS));

  Log.notice(F(CR "******************************************" CR));  
  Log.notice("Setup completed!");
}

void loop() {
  static uint8_t currentMode = 0;
  static unsigned long tsMillis = millis();
  static bool boardLedOn = false;

  // Blink board LED
  digitalWrite(13, boardLedOn ? HIGH : LOW);
  boardLedOn = !boardLedOn;

  modes[currentMode]->draw(leds);

  FastLED.show(BRIGHTNESS);
  FastLED.delay(10);

  // Change modes every so often 
  if (millis() - tsMillis > CHANGE_MODE_SEC * 1000) {
    tsMillis = millis();
    currentMode++; 
    if (currentMode > modes.size()-1) {
      currentMode = 0;
    }
  }
}
