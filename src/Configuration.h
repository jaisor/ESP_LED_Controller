#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>

#define WIFI        // 2.4Ghz wifi access point
#define LED         // Individually addressible LED strip
//#define KEYPAD      // Buttons

#define EEPROM_CONFIGURATION_START 0    // First EEPROM byte to be used for storing the configuration

#ifdef WIFI
    #define WIFI_SSID "LEDTri8266"
    #define WIFI_PASS "password123"

    // If unable to connect, it will create a soft accesspoint
    #define WIFI_FALLBACK_SSID "LEDT" // device chip id will be suffixed
    #define WIFI_FALLBACK_PASS "password123"

    #define NTP_SERVER "pool.ntp.org"
    #define NTP_GMT_OFFSET_SEC -25200
    #define NTP_DAYLIGHT_OFFSET_SEC 0

    // Web server
    #define WEB_SERVER_PORT 80
#endif

#ifdef LED
    #define LED_CHANGE_MODE_SEC   60
    #define LED_PIN 2
    #define LED_STRIP_SIZE 68
    #define LED_EXTERNAL_LEAF_SIZE 68
    #define LED_BRIGHTNESS 1.0 // 0-1
    #define LED_TYPE WS2811
    #define LED_COLOR_ORDER GRB
#endif

struct configuration_t {

    char _loaded[7]; // used to check if EEPROM was empty, should be true

    #ifdef WIFI
        char wifiSsid[32];
        char wifiPassword[63];
        char ntpServer[128];
        long gmtOffset_sec = 0;
        int daylightOffset_sec = 3600;
    #endif

    #ifdef LED
        float ledBrightness;
        uint8_t ledMode;
        unsigned long ledDelayMs;
        unsigned long ledCycleModeMs;
    #endif
};

extern configuration_t configuration;

void EEPROM_saveConfig();
void EEPROM_loadConfig();
void EEPROM_wipe();

#endif