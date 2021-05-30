#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>


#ifndef ESP8266
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <FastLED.h>
#include <vector>

#include "wifi/WifiManager.h"
#include "modes/HoneyOrangeMode.h"
#include "modes/PaletteMode.h"

//#define LED_PIN_BOARD     2
#define LED_PIN_STRIP     2
#define NUM_LEDS          71

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

std::vector<CBaseMode*> modes;
CWifiManager *wifiManager;

void setup() {
  delay( 1000 ); // power-up safety delay

  Serial.begin(115200);
  while(!Serial && !Serial.available()){}
  randomSeed(analogRead(0));

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.noticeln("******************************************");  

  FastLED.addLeds<LED_TYPE, LED_PIN_STRIP, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(255 * configuration.ledBrightness);

#ifdef LED_PIN_BOARD
  pinMode(LED_PIN_BOARD, OUTPUT);
  digitalWrite(LED_PIN_BOARD, HIGH);
#endif

  EEPROM_loadConfig();

  wifiManager = new CWifiManager();

  modes.push_back(new CPaletteMode(NUM_LEDS, PartyColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, HeatColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, RainbowColors_p, 255 / NUM_LEDS ));
  modes.push_back(new CPaletteMode(NUM_LEDS, CloudColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, ForestColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, OceanColors_p, 255 / NUM_LEDS));
  modes.push_back(new CPaletteMode(NUM_LEDS, HeatColors_p, 255 / NUM_LEDS));
  modes.push_back(new CHoneyOrangeMode(NUM_LEDS));

  Log.noticeln("Setup completed!");
}

void loop() {
  static unsigned long tsMillis = millis();
  static bool boardLedOn = false;

  wifiManager->loop();

  // Blink board LED
  digitalWrite(13, boardLedOn ? HIGH : LOW);
  boardLedOn = !boardLedOn;

  //Log.infoln("mode: %i, bright: %i, delay: %l, cycle: %l", configuration.ledMode, 255 * configuration.ledBrightness, configuration.ledDelayMs, configuration.ledCycleModeMs);
  if (configuration.ledMode > modes.size()-1) {
    configuration.ledMode = 0;
  }

  modes[configuration.ledMode]->draw(leds);

  FastLED.show(255 * configuration.ledBrightness);
  FastLED.delay(configuration.ledDelayMs);

  if (configuration.ledCycleModeMs > 0) {
    // Change modes every so often 
    if (millis() - tsMillis > configuration.ledCycleModeMs) {
      tsMillis = millis();
      configuration.ledMode++; 
      if (configuration.ledMode > modes.size()-1) {
        configuration.ledMode = 0;
      }
    }
  }
}
