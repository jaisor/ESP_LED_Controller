#include <Arduino.h>
#include <EEPROM.h>
#include <version.h>
#include "Configuration.h"

configuration_t configuration;
#ifdef WEB_LOGGING
  StringPrint logStream;
#endif

uint8_t EEPROM_initAndCheckFactoryReset() {
  Log.noticeln("Configuration size: %i", sizeof(configuration_t));
  
  EEPROM.begin(sizeof(configuration_t) + EEPROM_FACTORY_RESET + 1);
  uint8_t resetCounter = EEPROM.read(EEPROM_FACTORY_RESET);

  Log.noticeln("Factory reset counter: %i", resetCounter);
  Log.noticeln("EEPROM length: %i", EEPROM.length());

  // Bump reset counter
  EEPROM.write(EEPROM_FACTORY_RESET, resetCounter + 1);
  EEPROM.commit();

  return resetCounter;
}

void EEPROM_clearFactoryReset() {
  #if defined(ESP32)
  portMUX_TYPE mx = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&mx);
  #endif
  
  EEPROM.write(EEPROM_FACTORY_RESET, 0);
  EEPROM.commit();

  #if defined(ESP32)
  taskEXIT_CRITICAL(&mx);
  #endif
}

void EEPROM_saveConfig() {
  #if defined(ESP32)
  portMUX_TYPE mx = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&mx);
  #endif
  
  Log.infoln("Saving configuration to EEPROM");
  EEPROM.put(EEPROM_CONFIGURATION_START, configuration);
  Log.verboseln("Committing EEPROM");
  EEPROM.commit();

  #if defined(ESP32)
  taskEXIT_CRITICAL(&mx);
  #endif
}

void EEPROM_loadConfig() {

  configuration = {};
  EEPROM.get(EEPROM_CONFIGURATION_START, configuration);

  Log.noticeln("Configuration loaded: %s", configuration._loaded);

  if (strcmp(configuration._loaded, "jaisor")) {
    // blank
    Log.infoln("Blank configuration, loading defaults");
    strcpy(configuration._loaded, "jaisor");
    strcpy(configuration.name, DEVICE_NAME);
    #ifdef LED
      configuration.ledMode = 0;
      configuration.ledCycleModeMs = LED_CHANGE_MODE_SEC * 1000;
      configuration.ledDelayMs = 10;
      configuration.ledBrightness = LED_BRIGHTNESS;
      configuration.ledStripSize = LED_STRIP_SIZE;
      configuration.psLedBrightness = 1.0f;
      configuration.psStartHour = 0;
      configuration.psEndHour = 0;
    #endif
     #ifdef WIFI
      strcpy(configuration.ntpServer, NTP_SERVER);
      configuration.gmtOffset_sec = NTP_GMT_OFFSET_SEC;
      configuration.daylightOffset_sec = NTP_DAYLIGHT_OFFSET_SEC;
      configuration.wifiPower = 78;
    #endif
  }

#ifdef LED
  if (isnan(configuration.ledBrightness)) {
    Log.verboseln("NaN brightness");
    configuration.ledBrightness = LED_BRIGHTNESS;
  }
  if (isnan(configuration.ledMode)) {
    Log.verboseln("NaN ledMode");
    configuration.ledMode = 0;
  }
  if (isnan(configuration.ledCycleModeMs)) {
    Log.verboseln("NaN ledCycleModeMs");
    configuration.ledCycleModeMs = 0;
  }
  if (isnan(configuration.ledDelayMs)) {
    Log.verboseln("NaN ledDelayMs");
    configuration.ledDelayMs = 10;
  }
  if (isnan(configuration.ledStripSize)) {
    Log.verboseln("NaN ledStripSize");
    configuration.ledStripSize = LED_STRIP_SIZE;
  }
  if (isnan(configuration.psLedBrightness)) {
    Log.verboseln("NaN power-save brightness");
    configuration.psLedBrightness = 1.0;
  }
  if (isnan(configuration.psStartHour)) {
    Log.verboseln("NaN power-save start hour");
    configuration.psStartHour = 0;
  }
  if (isnan(configuration.psEndHour)) {
    Log.verboseln("NaN power-save end hour");
    configuration.psEndHour = 0;
  }
#endif

#ifdef WIFI
  String wifiStr = String(configuration.wifiSsid);
  for (auto i : wifiStr) {
    if (!isAscii(i)) {
      Log.verboseln("Bad SSID, loading default: %s", wifiStr.c_str());
      strcpy(configuration.wifiSsid, "");
      break;
    }
  }
#endif

  Log.noticeln("Device name: %s", configuration.name);
  Log.noticeln("Version: %s", VERSION);
}

void EEPROM_wipe() {
  Log.warningln("Wiping configuration with size %i!", EEPROM.length());
  for (uint16_t i = 0; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

uint32_t CONFIG_getDeviceId() {
    // Create AP using fallback and chip ID
  uint32_t chipId = 0;
  #ifdef ESP32
    for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
  #elif ESP8266
    chipId = ESP.getChipId();
  #endif

  return chipId;
}

static unsigned long tMillisUp = millis();
unsigned long CONFIG_getUpTime() {  
  return millis() - tMillisUp;
}

static bool isIntLEDOn = false;
void intLEDOn() {
  if (configuration.ledEnabled) {
    #if (defined(SEEED_XIAO_M0) || defined(ESP8266))
      digitalWrite(INTERNAL_LED_PIN, LOW);
    #else
      digitalWrite(INTERNAL_LED_PIN, HIGH);
    #endif
    isIntLEDOn = true;
  }
}

void intLEDOff() {
  #if (defined(SEEED_XIAO_M0) || defined(ESP8266))
    digitalWrite(INTERNAL_LED_PIN, HIGH);
  #else
    digitalWrite(INTERNAL_LED_PIN, LOW);
  #endif
  isIntLEDOn = false;
}

void intLEDBlink(uint16_t ms) {
  if (isIntLEDOn) { intLEDOff(); } else { intLEDOn(); }
  delay(ms);
  if (isIntLEDOn) { intLEDOff(); } else { intLEDOn(); }
}

#if defined(TEMP_SENSOR)
  float _correct(sensorCorrection c[], float measured) {
    if (c[0].measured + c[0].actual + c[1].measured + c[1].actual == 0) {
      return measured;
    }
    float a = (c[1].actual-c[0].actual) / (c[1].measured-c[0].measured);
    float b = c[0].actual - a * c[0].measured;
    return a * measured + b;
  }

  float correctT(float measured) {
    return _correct(configuration.tCorrection, measured);
  }

  float correctH(float measured) {
    return _correct(configuration.hCorrection, measured);
  }
#endif

#ifdef LED
float currentLedBrightness = 0;
unsigned long tsLedBrightnessUpdate = 0;

bool isInsideInterval(int i, int8_t s, int8_t e) {
  if (s <= e) {
    return i>=s && i<e;
  } else {
    return ((i>=s && i<24) || (i>=0 && i<e));
  }
}

float CONFIG_getLedBrightness(bool force) {
  #ifdef WIFI
  // Check on power save mode about once per minute
  if (configuration.psLedBrightness < 1.0f && (configuration.psStartHour || configuration.psEndHour) && (force || millis() - tsLedBrightnessUpdate > 60000)) {
    tsLedBrightnessUpdate = millis();
    struct tm timeinfo;
    bool timeUpdated = getLocalTime(&timeinfo);
    if (timeUpdated && isInsideInterval(timeinfo.tm_hour, configuration.psStartHour, configuration.psEndHour)) {
        currentLedBrightness = configuration.ledBrightness * configuration.psLedBrightness;
        if (currentLedBrightness != configuration.ledBrightness) {
          Log.infoln("Current LED brightness is '%D' compared to default '%D'", currentLedBrightness, configuration.ledBrightness);
        }
    } else {
      currentLedBrightness = configuration.ledBrightness;
    }
  }
  #else
    currentLedBrightness = configuration.ledBrightness;
  #endif
  return currentLedBrightness;
}
#endif
