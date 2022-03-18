#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>


#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <FastLED.h>
#include <vector>

#include "wifi/WifiManager.h"
#include "modes/HoneyOrangeMode.h"
#include "modes/PaletteMode.h"
#include "modes/ColorSplitMode.h"
#include "modes/WhiteLightMode.h"
#include "modes/SlavaUkrainiRingMode.h"

CRGB* leds;

std::vector<CBaseMode*> modes;
CWifiManager *wifiManager;

unsigned long tsSmoothBoot;
bool smoothBoot;

void setup() {
  delay( 1000 ); // power-up safety delay

  Serial.begin(115200);
  while(!Serial && !Serial.available()){}
  randomSeed(analogRead(0));

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.noticeln("******************************************");  

#ifdef LED_PIN_BOARD
  pinMode(LED_PIN_BOARD, OUTPUT);
  digitalWrite(LED_PIN_BOARD, HIGH);
#endif

  if (EEPROM_initAndCheckFactoryReset() >= 3) {
    Log.warningln("Factory reset conditions met!");
    EEPROM_wipe();    
  }

  tsSmoothBoot = millis();
  smoothBoot = false;

  EEPROM_loadConfig();

  leds = new CRGB[configuration.ledStripSize];

  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(255);

  wifiManager = new CWifiManager();

  modes.push_back(new CWhiteLightMode(configuration.ledStripSize, "White Light"));
  modes.push_back(new CSlavaUkrainiRingMode(configuration.ledStripSize, "Slava Ukraini"));
  modes.push_back(new CColorSplitMode(configuration.ledStripSize, "Dual Ring"));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Party Colors", PartyColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Heat Colors", HeatColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Rainbow Colors", RainbowColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Cloud Colors", CloudColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Forest Colors", ForestColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Ocean Colors", OceanColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Lava Colors", LavaColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CHoneyOrangeMode(configuration.ledStripSize, "Honey Amber"));
  
  wifiManager->setModes(&modes);

  Log.noticeln("Setup completed!");
}

void loop() {
  static unsigned long tsMillis = millis();

  if (!smoothBoot && millis() - tsSmoothBoot > FACTORY_RESET_CLEAR_TIMER_MS) {
    smoothBoot = true;
    EEPROM_clearFactoryReset();
    Log.noticeln("Device booted smoothly!");
  }
  
  wifiManager->loop();

  if (wifiManager->isRebootNeeded()) {
    return;
  }

  if (configuration.ledMode > modes.size()-1) {
    configuration.ledMode = 0;
  }

  modes[configuration.ledMode]->draw(leds);
  FastLED.show(255 * configuration.ledBrightness);

  if (configuration.ledCycleModeMs > 0) {
    // Change modes every so often 
    if (millis() - tsMillis > configuration.ledCycleModeMs) {
      tsMillis = millis();
      configuration.ledMode++; 
      if (configuration.ledMode > modes.size()-1) {
        configuration.ledMode = 0;
      }
      Log.verboseln("Switching modes to '%s'", modes[configuration.ledMode]->getName().c_str());
    }
  }

  delay(50);
}
