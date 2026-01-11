#pragma once

#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>
#include <StreamUtils.h>

#define WIFI        // 2.4Ghz wifi access point
#define LED         // Individually addressible LED strip
//#define KEYPAD      // Buttons
//#define RING_LIGHT

//#define DEBUG_MOCK_HP
//#define DISABLE_LOGGING
#ifndef DISABLE_LOGGING
  #define LOG_LEVEL LOG_LEVEL_VERBOSE
#endif

//#define WEB_LOGGING // When enabled log is available at http://<device_ip>/log
#ifdef WEB_LOGGING
  #define WEB_LOG_LEVEL LOG_LEVEL_VERBOSE
#endif

#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define SERIAL_MONITOR_BAUD 460800
  #define DISABLE_LOGGING // Xiao's setup with USB requires serial to be initialized on the IDE else it blocks
#else
  #define SERIAL_MONITOR_BAUD 115200
#endif

#define EEPROM_FACTORY_RESET 0           // Byte to be used for factory reset device fails to start or is rebooted within 1 sec 3 consequitive times
#define EEPROM_CONFIGURATION_START 1     // First EEPROM byte to be used for storing the configuration

#define FACTORY_RESET_CLEAR_TIMER_MS 3000   // Clear factory reset counter when elapsed, considered smooth boot

#ifdef ESP32
  #define DEVICE_NAME "ESP32LED"
#elif ESP8266
  #define DEVICE_NAME "ESP8266LED"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define INTERNAL_LED_PIN GPIO_NUM_8
#else
  #define INTERNAL_LED_PIN LED_BUILTIN
#endif


#ifdef WIFI
    #define WIFI_SSID DEVICE_NAME
    #define WIFI_PASS "password123"

    // If unable to connect, it will create a soft accesspoint
    #define WIFI_FALLBACK_SSID DEVICE_NAME // device chip id will be suffixed
    #define WIFI_FALLBACK_PASS "password123"

    #define NTP_SERVER "pool.ntp.org"
    #define NTP_GMT_OFFSET_SEC -25200
    #define NTP_DAYLIGHT_OFFSET_SEC 0

    // Web server
    #define WEB_SERVER_PORT 80
#endif

#ifdef LED
    #define LED_CHANGE_MODE_SEC   60
    #ifdef ESP32
        #define LED_PIN GPIO_NUM_12
    #elif ESP8266
        #define LED_PIN D3
    #endif
    // 267 for RingLight, 480 for PingPong table light
    #ifdef RING_LIGHT 
        #define LED_STRIP_SIZE 267
        #define OUTTER_RING_SIZE 141
    #else
        #define LED_STRIP_SIZE 61  
        #define OUTTER_RING_SIZE 240
    #endif
    #define LED_BRIGHTNESS 0.1  // 0-1, 1-max brightness, make sure your LEDs are powered accordingly
    #define LED_COLOR_ORDER GRB
#endif

struct configuration_t {

    #ifdef WIFI
        char wifiSsid[32];
        char wifiPassword[63];

        int8_t wifiPower;

        // ntp
        char ntpServer[128];
        long gmtOffset_sec;
        int daylightOffset_sec;
    #endif

    #ifdef LED
        float ledBrightness;
        uint8_t ledMode;
        uint8_t ledType;
        unsigned long ledDelayMs;
        unsigned long ledCycleModeMs;
        uint16_t ledStripSize;
        float psLedBrightness;
        int8_t psStartHour;
        int8_t psEndHour;
        uint8_t cycleModesCount;
        uint8_t cycleModesList[32];  // Up to 32 modes in cycle list
    #endif

    char name[128];

    uint8_t ledEnabled;

    char _loaded[7]; // used to check if EEPROM was correctly set    
};

extern configuration_t configuration;
#ifdef WEB_LOGGING
  extern StringPrint logStream;
#endif

uint8_t EEPROM_initAndCheckFactoryReset();
void EEPROM_clearFactoryReset();

void EEPROM_saveConfig();
void EEPROM_loadConfig();
void EEPROM_wipe();

uint32_t CONFIG_getDeviceId();
unsigned long CONFIG_getUpTime();

void intLEDOn();
void intLEDOff();
void intLEDBlink(uint16_t ms);

#ifdef LED
    float CONFIG_getLedBrightness(bool force = false);
#endif
