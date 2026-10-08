#include <Arduino.h>
#include <EEPROM.h>
#include <version.h>
#include "Configuration.h"
#ifdef LED
  #include <FastLED.h>
#endif

configuration_t configuration;
#ifdef WEB_LOGGING
  StringPrint logStream;

  void trimLogStream() {
    if (logStream.str().length() > WEB_LOG_MAX_SIZE) {
      // Keep only the most recent half of the buffer
      String s = logStream.str().c_str();
      logStream.str("");
      logStream.print(s.substring(s.length() - WEB_LOG_MAX_SIZE / 2));
    }
  }
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
  //portMUX_TYPE mx = portMUX_INITIALIZER_UNLOCKED;
  //taskENTER_CRITICAL(&mx);
  #endif
  
  EEPROM.write(EEPROM_FACTORY_RESET, 0);
  EEPROM.commit();

  #if defined(ESP32)
  //taskEXIT_CRITICAL(&mx);
  #endif
}

void EEPROM_saveConfig() {
  Log.info("Saving configuration to EEPROM ... ");

  #if defined(ESP32)
  //portMUX_TYPE mx = portMUX_INITIALIZER_UNLOCKED;
  //taskENTER_CRITICAL(&mx);
  #endif
  
  EEPROM.put(EEPROM_CONFIGURATION_START, configuration);
  EEPROM.commit();

  #if defined(ESP32)
  //taskEXIT_CRITICAL(&mx);
  #endif
  
  Log.verboseln("Saved");
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
      configuration.ledType = 0; // Default to WS281B
      configuration.ledCycleModeMs = LED_CHANGE_MODE_SEC * 1000;
      configuration.ledDelayMs = 10;
      configuration.ledBrightness = LED_BRIGHTNESS;
      configuration.ledStripSize = LED_STRIP_SIZE;
      configuration.psLedBrightness = 1.0f;
      configuration.psStartHour = 0;
      configuration.psEndHour = 0;
      configuration.cycleModesCount = 0;  // 0 means cycle through all modes
      for (int i = 0; i < 32; i++) {
        configuration.cycleModesList[i] = 0;
      }
    #endif
    #ifdef WIFI
      strcpy(configuration.ntpServer, NTP_SERVER);
      configuration.gmtOffset_sec = NTP_GMT_OFFSET_SEC;
      configuration.daylightOffset_sec = NTP_DAYLIGHT_OFFSET_SEC;
      #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        configuration.wifiPower = 34; // ESP32-C3 default: WIFI_POWER_8_5dBm (8.5dBm)
      #else
        configuration.wifiPower = 82;
      #endif
    #endif
    configuration.ledEnabled = false;
    #ifdef LED
      configuration._ledLoaded[0] = '\0';
    #endif
  }

#ifdef LED
  if (strcmp(configuration._ledLoaded, "led")) {
    // Blank, or saved by firmware that had the LED hardware compiled in - use those same defaults
    Log.infoln("No LED hardware configuration, loading defaults");
    strcpy(configuration._ledLoaded, "led");
    configuration.ledLayout = LED_LAYOUT;
    configuration.ledPin = LED_PIN;
    configuration.ledPin2 = LED_PIN_2;
    configuration.ledColorOrder = static_cast<uint8_t>(LED_COLOR_ORDER);
    configuration.ledStripSize2 = LED_STRIP_SIZE_2;
    configuration.ledRingOuterSize = LED_RING_OUTER_SIZE;
    configuration.ledMirror = false;
  }
#endif

#ifdef LED
  if (isnan(configuration.ledBrightness)) {
    Log.verboseln("NaN brightness");
    configuration.ledBrightness = LED_BRIGHTNESS;
  }
  if (isnan(configuration.ledMode)) {
    Log.verboseln("NaN ledMode");
    configuration.ledMode = 0;
  }
  if (isnan(configuration.ledType)) {
    Log.verboseln("NaN ledType");
    configuration.ledType = 0;
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
  if (isnan(configuration.cycleModesCount) || configuration.cycleModesCount > 32) {
    Log.verboseln("Invalid cycleModesCount");
    configuration.cycleModesCount = 0;
  }

  // LED hardware - a bad value here means no light at all, so repair field by field
  if (configuration.ledType >= LED_TYPE_COUNT) {
    Log.verboseln("Invalid ledType");
    configuration.ledType = 0;
  }
  if (configuration.ledLayout >= LED_LAYOUT_COUNT) {
    Log.verboseln("Invalid ledLayout");
    configuration.ledLayout = LED_LAYOUT;
  }
  if (!CONFIG_isValidLedPin(configuration.ledPin)) {
    Log.verboseln("Invalid ledPin");
    configuration.ledPin = LED_PIN;
  }
  if (!CONFIG_isValidLedPin(configuration.ledPin2)) {
    Log.verboseln("Invalid ledPin2");
    configuration.ledPin2 = LED_PIN_2;
  }
  if (CONFIG_ledColorOrderName(configuration.ledColorOrder) == nullptr) {
    Log.verboseln("Invalid ledColorOrder");
    configuration.ledColorOrder = static_cast<uint8_t>(LED_COLOR_ORDER);
  }
  if (configuration.ledStripSize == 0 || configuration.ledStripSize > LED_MAX_COUNT) {
    Log.verboseln("Invalid ledStripSize");
    configuration.ledStripSize = LED_STRIP_SIZE;
  }
  if (configuration.ledStripSize2 == 0 || configuration.ledStripSize2 > LED_MAX_COUNT) {
    Log.verboseln("Invalid ledStripSize2");
    configuration.ledStripSize2 = LED_STRIP_SIZE_2;
  }
  configuration.ledMirror = configuration.ledMirror ? 1 : 0;
  if (configuration.ledLayout == LED_LAYOUT_RING && configuration.ledStripSize >= 4) {
    configuration.ledRingOuterSize = constrain(configuration.ledRingOuterSize, 2, configuration.ledStripSize - 2);
  }
  const char *ledError = CONFIG_checkLedHardware(configuration);
  if (ledError) {
    // Whatever is left (pins shared between strips, too many LEDs, ring too small) - fall back to one strip
    Log.warningln("LED hardware configuration invalid (%s), using a single strip", ledError);
    configuration.ledLayout = LED_LAYOUT_SINGLE;
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

String CONFIG_getChipModel() {
  #ifdef ESP32
    return ESP.getChipModel();
  #elif ESP8266
    return "ESP8266";
  #endif
}

uint8_t CONFIG_getChipRevision() {
  #ifdef ESP32
    return ESP.getChipRevision();
  #elif ESP8266
    return 0; // not exposed by the ESP8266 core
  #endif
}

uint32_t CONFIG_getFlashChipSize() {
  #ifdef ESP32
    return ESP.getFlashChipSize();
  #elif ESP8266
    return ESP.getFlashChipRealSize();
  #endif
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
  } else {
    intLEDOff();
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
  if (force || millis() - tsLedBrightnessUpdate > 60000) {
    tsLedBrightnessUpdate = millis();
    struct tm timeinfo;
    if (configuration.psStartHour || configuration.psEndHour) {
      bool timeUpdated = getLocalTime(&timeinfo, 0); // 0ms timeout - non-blocking, use cached system time only
      if (timeUpdated && isInsideInterval(timeinfo.tm_hour, configuration.psStartHour, configuration.psEndHour)) {
          currentLedBrightness = configuration.ledBrightness * configuration.psLedBrightness;
          if (currentLedBrightness != configuration.ledBrightness) {
            Log.infoln("Current LED brightness is '%D' compared to default '%D'", currentLedBrightness, configuration.ledBrightness);
          }
      } else {
        currentLedBrightness = configuration.ledBrightness;
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

#ifdef LED
const char * const LED_TYPE_NAMES[] = {"WS2812B", "WS2812", "WS2813", "WS2815", "SK6812", "TM1809", "TM1804", "TM1803", "UCS1903", "UCS1904", "GS1903", "PL9823", "WS2852", "WS2811"};
const uint8_t LED_TYPE_COUNT = sizeof(LED_TYPE_NAMES) / sizeof(LED_TYPE_NAMES[0]);

#define LED_COLOR_ORDER_ENTRY(order) { static_cast<uint8_t>(order), #order }
const led_option_t LED_COLOR_ORDERS[] = {
  LED_COLOR_ORDER_ENTRY(RGB), LED_COLOR_ORDER_ENTRY(RBG), LED_COLOR_ORDER_ENTRY(GRB),
  LED_COLOR_ORDER_ENTRY(GBR), LED_COLOR_ORDER_ENTRY(BRG), LED_COLOR_ORDER_ENTRY(BGR)
};
#undef LED_COLOR_ORDER_ENTRY
const uint8_t LED_COLOR_ORDER_COUNT = sizeof(LED_COLOR_ORDERS) / sizeof(LED_COLOR_ORDERS[0]);

const char * const LED_LAYOUT_IDS[] = {"single", "dual", "ring"};
const char * const LED_LAYOUT_LABELS[] = {"Single strip", "Dual strip", "Ring light"};
static_assert(sizeof(LED_LAYOUT_IDS) / sizeof(LED_LAYOUT_IDS[0]) == LED_LAYOUT_COUNT, "LED_LAYOUT_IDS out of sync");
static_assert(sizeof(LED_LAYOUT_LABELS) / sizeof(LED_LAYOUT_LABELS[0]) == LED_LAYOUT_COUNT, "LED_LAYOUT_LABELS out of sync");

#define LED_PIN_ENTRY(pin) pin,
const uint8_t LED_PINS[] = { LED_PIN_LIST(LED_PIN_ENTRY) };
#undef LED_PIN_ENTRY
const uint8_t LED_PIN_COUNT = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

const char* CONFIG_ledColorOrderName(uint8_t order) {
  for (uint8_t i = 0; i < LED_COLOR_ORDER_COUNT; i++) {
    if (LED_COLOR_ORDERS[i].value == order) {
      return LED_COLOR_ORDERS[i].name;
    }
  }
  return nullptr;
}

int CONFIG_ledColorOrderFromName(const char *name) {
  for (uint8_t i = 0; name && i < LED_COLOR_ORDER_COUNT; i++) {
    if (!strcasecmp(LED_COLOR_ORDERS[i].name, name)) {
      return LED_COLOR_ORDERS[i].value;
    }
  }
  return -1;
}

int CONFIG_ledLayoutFromName(const char *name) {
  for (uint8_t i = 0; name && i < LED_LAYOUT_COUNT; i++) {
    if (!strcasecmp(LED_LAYOUT_IDS[i], name)) {
      return i;
    }
  }
  return -1;
}

bool CONFIG_isValidLedPin(uint8_t pin) {
  // Pins the firmware drives for something else
  if (pin == INTERNAL_LED_PIN) return false;
  #if (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)) && defined(OLED)
    if (pin == GPIO_NUM_5 || pin == GPIO_NUM_6) return false;  // OLED I2C, see CDevice
  #endif
  #ifdef BUTTONS
    if (pin == BUTTON_1_PIN || pin == BUTTON_2_PIN) return false;
  #endif
  for (uint8_t i = 0; i < LED_PIN_COUNT; i++) {
    if (LED_PINS[i] == pin) {
      return true;
    }
  }
  return false;
}

uint16_t CONFIG_getLedCount(const configuration_t &c) {
  if (c.ledLayout == LED_LAYOUT_DUAL) {
    return c.ledStripSize + (c.ledMirror ? c.ledStripSize : c.ledStripSize2);
  }
  return c.ledStripSize;
}

const char* CONFIG_checkLedHardware(const configuration_t &c) {
  if (c.ledLayout >= LED_LAYOUT_COUNT) return "unknown layout";
  if (c.ledType >= LED_TYPE_COUNT) return "unknown LED chipset";
  if (!CONFIG_ledColorOrderName(c.ledColorOrder)) return "unknown color order";
  if (!CONFIG_isValidLedPin(c.ledPin)) return "data pin not available on this board";
  if (c.ledStripSize == 0) return "number of LEDs must be at least 1";
  if (c.ledLayout == LED_LAYOUT_DUAL) {
    if (!CONFIG_isValidLedPin(c.ledPin2)) return "second strip data pin not available on this board";
    if (c.ledPin2 == c.ledPin) return "each strip needs its own data pin";
    if (!c.ledMirror && c.ledStripSize2 == 0) return "second strip needs at least 1 LED";
  }
  if (c.ledLayout == LED_LAYOUT_RING) {
    if (c.ledStripSize < 4) return "a ring light needs at least 4 LEDs";
    if (c.ledRingOuterSize < 2 || c.ledRingOuterSize > c.ledStripSize - 2) return "each ring needs at least 2 LEDs";
  }
  if ((uint32_t)c.ledStripSize + (c.ledLayout == LED_LAYOUT_DUAL ? (c.ledMirror ? c.ledStripSize : c.ledStripSize2) : 0) > LED_MAX_COUNT) {
    return "too many LEDs";
  }
  return nullptr;
}
#endif
