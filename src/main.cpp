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
#include "modes/HalfwayPaletteMode.h"
#include "modes/RingPaletteMode.h"
#include "modes/ColorSplitMode.h"
#include "modes/SlavaUkrainiRingMode.h"
#include "modes/ChristmasRunningMode.h"

#include "modes/WhiteLightMode.h"
#include "modes/PixelSeparatorMode.h"

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

const TProgmemRGBPalette16 Christmas_p FL_PROGMEM =
{
    0x00FF00,   // Bright Green
    0x00CC00,   // Green
    0x009900,   // Dark Green
    0x228B22,   // Forest Green

    0x32CD32,   // Lime Green
    0x4CBB17,   // Kelly Green
    0xFF6347,   // Tomato Red
    0xFF4500,   // Orange Red

    0xFF0000,   // Red
    0xDC143C,   // Crimson
    0xB22222,   // Fire Brick
    0x8B0000,   // Dark Red

    0xA52A2A,   // Brown
    0xFF0000,   // Red
    0x00AA00,   // Medium Green
    0x00FF00,   // Bright Green
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

  // Initialize FastLED based on configured LED type
  switch(configuration.ledType) {
    case 0:  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 1:  FastLED.addLeds<WS2812, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 2:  FastLED.addLeds<WS2813, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 3:  FastLED.addLeds<WS2815, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 4:  FastLED.addLeds<SK6812, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 5:  FastLED.addLeds<TM1809, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 6:  FastLED.addLeds<TM1804, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 7:  FastLED.addLeds<TM1803, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 8:  FastLED.addLeds<UCS1903, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 9:  FastLED.addLeds<UCS1904, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 10: FastLED.addLeds<GS1903, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 11: FastLED.addLeds<PL9823, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 12: FastLED.addLeds<WS2852, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    case 13: FastLED.addLeds<WS2811, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break;
    default: FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, configuration.ledStripSize).setCorrection( TypicalLEDStrip ); break; // Default to WS2812B
  }
  
  Log.infoln("LED Type configured: %d", configuration.ledType);
  FastLED.setBrightness(255);
  CONFIG_getLedBrightness(true);

  wifiManager = new CWifiManager();
  
  #ifdef RING_LIGHT
  modes.push_back(new CSlavaUkrainiRingMode(configuration.ledStripSize, "Slava Ukraini"));
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
  /*
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Party Colors", PartyColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Heat Colors", HeatColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Rainbow Colors", RainbowColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Cloud Colors", CloudColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Forest Colors", ForestColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Ocean Colors", OceanColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Lava Colors", LavaColors_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Halfway Rainbow", RainbowColors_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Halfway Party", PartyColors_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  */
  modes.push_back(new CPaletteMode(configuration.ledStripSize, "Christmas Gradual", Christmas_p, 255.0 / (float)configuration.ledStripSize));
  modes.push_back(new CHalfwayPaletteMode(configuration.ledStripSize, "Christmas Halfway", Christmas_p, 255.0 / ((float)configuration.ledStripSize / 2.0)));
  modes.push_back(new CChristmasRunningMode(configuration.ledStripSize, "Christmas Running"));
  modes.push_back(new CPixelSeparatorMode(configuration.ledStripSize, "Pixel Separator"));
  
  wifiManager->setModes(&modes);

  Log.noticeln("Setup completed!");
}

void loop() {
  static unsigned long tsMillis = millis();

  if (!smoothBoot && millis() - tsSmoothBoot > FACTORY_RESET_CLEAR_TIMER_MS) {
    smoothBoot = true;
    EEPROM_clearFactoryReset();
    Log.noticeln("Device booted smoothly!");
    Log.verboseln("LED brightness: '%i'", 255 * CONFIG_getLedBrightness());
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
  yield();
}
