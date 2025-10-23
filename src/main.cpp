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
#include "modes/RingPaletteMode.h"
#include "modes/ColorSplitMode.h"
#include "modes/SlavaUkrainiRingMode.h"

#include "modes/WhiteLightMode.h"

CRGB* leds;

std::vector<CBaseMode*> modes;
CWifiManager *wifiManager;

unsigned long tsSmoothBoot;
bool smoothBoot;

#define S1C1 0xFFD700
#define S1C2 0x665700
#define S2C1 0x0057B8
#define S2C2 0x003066

const TProgmemRGBPalette16 SlavaUkraini_p FL_PROGMEM =
{
    S1C2,
    S1C2,
    S1C1,
    S1C1,

    S2C1,
    S2C1,
    S2C2,
    S2C2,

    S2C2,
    S2C2,
    S2C1,
    S2C1,

    S1C1,
    S1C1,
    S1C2,
    S1C2,
};

const TProgmemRGBPalette16 Pride_p FL_PROGMEM =
{
    0xFF0018,   // Vivid Red
    0xFF0018,   // Vivid Red
    0xFFA52C,   // Deep Saffron
    0xFFA52C,   // Deep Saffron

    0xFFFF41,   // Maximum Yellow
    0xFFFF41,   // Maximum Yellow
    0x008018,   // Ao
    0x008018,   // Ao

    0x0000F9,   // Blue
    0x0000F9,   // Blue
    0x86007D,   // Philippine Violet
    0x86007D,   // Philippine Violet

    0x86007D,   // Philippine Violet
    0x86007D,   // Philippine Violet
    0xFF0018,   // Vivid Red
    0xFF0018,   // Vivid Red
};

void setup() {
  randomSeed(analogRead(0));
  
  #ifdef ESP8266
    pinMode(D0, WAKEUP_PULLUP);
  #endif
  pinMode(INTERNAL_LED_PIN, OUTPUT);
  intLEDOn();

  #ifndef DISABLE_LOGGING
  Serial.begin(SERIAL_MONITOR_BAUD); while (!Serial); delay(100);
  Log.begin(LOG_LEVEL, &Serial);
  Log.infoln(F("\n\nInitializing..."));
    #ifdef WEB_LOGGING
    Log.addHandler(&logStream);
    Log.infoln(F("Initializing web log..."));
    #endif
  #elif defined(WEB_LOGGING)
    Log.begin(WEB_LOG_LEVEL, &logStream);
    Log.infoln(F("Initializing web log..."));
  #endif

  if (EEPROM_initAndCheckFactoryReset() >= 3) {
    Log.warningln("Factory reset conditions met!");
    EEPROM_wipe();  
  }

  tsSmoothBoot = millis();
  smoothBoot = false;

  EEPROM_loadConfig();

  Log.infoln("Configuration loaded");

  leds = new CRGB[configuration.ledStripSize];

  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(255);

  wifiManager = new CWifiManager();

  modes.push_back(new CWhiteLightMode(configuration.ledStripSize, "White Light"));
  modes.push_back(new CSlavaUkrainiRingMode(configuration.ledStripSize, "Slava Ukraini"));

  #ifdef RING_LIGHT
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Slava Ukraini 2", SlavaUkraini_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CColorSplitMode(configuration.ledStripSize, "Dual Ring"));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Party Colors", PartyColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Heat Colors", HeatColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Rainbow Colors", RainbowColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Cloud Colors", CloudColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Forest Colors", ForestColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Ocean Colors", OceanColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Lava Colors", LavaColors_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  modes.push_back(new CHoneyOrangeMode(configuration.ledStripSize, "Honey Amber"));
  modes.push_back(new CRingPaletteMode(configuration.ledStripSize, OUTTER_RING_SIZE, "Pride", Pride_p, 255.0 / ((float)configuration.ledStripSize) * 2.0));
  #endif

  modes.push_back(new CWhiteLightMode(configuration.ledStripSize, "White Light"));
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
  FastLED.show(255 * CONFIG_getLedBrightness());

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

  delay(5);
}
