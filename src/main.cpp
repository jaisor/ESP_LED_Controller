#include <Arduino.h>
#include <ArduinoLog.h>


#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include "wifi/WifiManager.h"
#include "Device.h"
#include "LedManager.h"

CWifiManager *wifiManager;
CDevice *device;
CLEDManager *ledManager;

unsigned long tsSmoothBoot;
bool smoothBoot;

void setup() {
  randomSeed(analogRead(0));
  
  #ifdef ESP8266
    pinMode(D0, WAKEUP_PULLUP);
  #endif
  pinMode(INTERNAL_LED_PIN, OUTPUT);

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
  intLEDOn();

  device = new CDevice();
  device->setBootProgress(20);

  wifiManager = new CWifiManager();
  wifiManager->setDevice(device);
  device->setBootProgress(50);

  ledManager = new CLEDManager();
  ledManager->setModeChangeCallback([]() {
    if (wifiManager) {
      wifiManager->updateModeChangeTime();
    }
  });
  ledManager->setChargingStartCallback([]() {
    #ifdef OLED
    if (device) {
      device->displayTemporaryMessage("Charging...", 5000);
    }
    #endif
  });
  ledManager->setup();
  device->setBootProgress(80);

  wifiManager->setModes(ledManager->getModes());
  wifiManager->updateModeChangeTime();

  device->setBootProgress(100);
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
  
  device->loop();
  wifiManager->loop();
  ledManager->loop();

  #ifdef WEB_LOGGING
  trimLogStream();
  #endif

  if (wifiManager->isRebootNeeded()) {
    return;
  }

  delay(5);
  yield();
}
