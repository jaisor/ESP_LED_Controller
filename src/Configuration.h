#pragma once

#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>
#include <StreamUtils.h>

#define WIFI        // 2.4Ghz wifi access point
#define LED         // Individually addressible LED strip
#define OLED        // OLED display
#define BUTTONS     // Buttons

//#define KEYPAD      // Buttons
//#define RING_LIGHT

//#define DEBUG_MOCK_HP
//#define DISABLE_LOGGING
#ifndef DISABLE_LOGGING
  #define LOG_LEVEL LOG_LEVEL_VERBOSE
#endif

#define WEB_LOGGING // When enabled log is available at http://<device_ip>/log
#ifdef WEB_LOGGING
  #define WEB_LOG_LEVEL LOG_LEVEL_VERBOSE
  #define WEB_LOG_MAX_SIZE 8192  // Cap log buffer to 8KB to prevent heap exhaustion
#endif

#ifdef CONFIG_IDF_TARGET_ESP32C3
  #define SERIAL_MONITOR_BAUD 460800
  #define DISABLE_LOGGING // Xiao's setup with USB requires serial to be initialized on the IDE else it blocks
#else
  #define SERIAL_MONITOR_BAUD 115200
#endif

#define EEPROM_FACTORY_RESET 0           // Byte to be used for factory reset device fails to start or is rebooted within 1 sec 3 consequitive times
#define EEPROM_CONFIGURATION_START 1     // First EEPROM byte to be used for storing the configuration

#define FACTORY_RESET_CLEAR_TIMER_MS 2000   // Clear factory reset counter when elapsed, considered smooth boot

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define INTERNAL_LED_PIN GPIO_NUM_8
  #define DEVICE_NAME "ESP32C3LED"
#elif defined(SEEED_XIAO_M0)
  #define INTERNAL_LED_PIN     13
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define INTERNAL_LED_PIN LED_BUILTIN
  #define DEVICE_NAME "ESP32LED"
#elif defined(ESP8266)
  #define INTERNAL_LED_PIN LED_BUILTIN
  #define DEVICE_NAME "ESP8266LED"
#else
  #define INTERNAL_LED_PIN LED_BUILTIN
  #define DEVICE_NAME "ESPXXLED"
#endif

#ifdef WIFI
    #define WIFI_SSID DEVICE_NAME
    #define WIFI_PASS "password123"

    // If unable to connect, it will create a soft accesspoint
    #define WIFI_FALLBACK_SSID DEVICE_NAME // device chip id will be suffixed
    #define WIFI_FALLBACK_PASS "password123"

    #define NTP_SERVER "pool.ntp.org"
    #define NTP_GMT_OFFSET_SEC -28800  // Pacific Standard Time (UTC-8), DST handled automatically
    #define NTP_DAYLIGHT_OFFSET_SEC 0  // Deprecated, kept for compatibility

    // Web server
    #define WEB_SERVER_PORT 80
#endif

#ifdef LED
    #define LED_CHANGE_MODE_SEC   0
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
      #define LED_PIN GPIO_NUM_2
    #elif defined(SEEED_XIAO_M0)
      #define LED_PIN 10
    #elif defined(CONFIG_IDF_TARGET_ESP32)
      #define LED_PIN GPIO_NUM_12
    #elif defined(ESP8266)
        #define LED_PIN D3
    #else
      #define LED_PIN 1
    #endif
    // 267 for RingLight, 480 for PingPong table light
    #ifdef RING_LIGHT 
        #define LED_STRIP_SIZE 267
        #define OUTTER_RING_SIZE 141
    #else
        #define LED_STRIP_SIZE 90  
        #define OUTTER_RING_SIZE 240
    #endif
    #define LED_BRIGHTNESS 1  // 0-1, 1-max brightness, make sure your LEDs are powered accordingly
    #define LED_COLOR_ORDER GRB
#endif

#ifdef BUTTONS
  #define BUTTON_1_PIN     GPIO_NUM_0
  #define BUTTON_2_PIN     GPIO_NUM_1
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #ifdef OLED // ESD32C3 has a built-in OLED ie ESP32C4 Dev board
    #define OLED_SCREEN_WIDTH 72 // OLED display width, in pixels
    #define OLED_SCREEN_HEIGHT 40 // OLED display height, in pixel
    #define OLED_I2C_ID  0x3C
  #endif
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
  void trimLogStream();
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
