#pragma once

#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>
#include <StreamUtils.h>

#define WIFI        // 2.4Ghz wifi access point
#define LED         // Individually addressible LED strip
//#define OLED        // OLED display
//#define BUTTONS     // Buttons

//#define KEYPAD      // Buttons

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

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  #define SERIAL_MONITOR_BAUD 460800
  //#define DISABLE_LOGGING // Xiao's setup with USB requires serial to be initialized on the IDE else it blocks
#else
  #define SERIAL_MONITOR_BAUD 115200
#endif

#define EEPROM_FACTORY_RESET 0           // Byte to be used for factory reset device fails to start or is rebooted within 1 sec 3 consequitive times
#define EEPROM_CONFIGURATION_START 1     // First EEPROM byte to be used for storing the configuration

#define FACTORY_RESET_CLEAR_TIMER_MS 2000   // Clear factory reset counter when elapsed, considered smooth boot

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define INTERNAL_LED_PIN GPIO_NUM_8
  #define DEVICE_NAME "ESP32C3LED"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define INTERNAL_LED_PIN LED_BUILTIN
  #define DEVICE_NAME "ESP32S3LED"
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
    #define NTP_GMT_OFFSET_SEC 0  // GMT (default on factory reset)
    #define NTP_DAYLIGHT_OFFSET_SEC 0  // Deprecated, kept for compatibility

    // Web server
    #define WEB_SERVER_PORT 80

    // MQTT, disabled until a server is set on the Device page. Topics live under <MQTT_TOPIC>/<device id>.
    #define MQTT_PORT 1883
    #define MQTT_TOPIC "esp_led"
    #define MQTT_DISCOVERY_PREFIX "homeassistant"   // Home Assistant's default discovery prefix
    #define MQTT_RECONNECT_MS 60000     // Connecting blocks the loop while the broker is unreachable, so retry sparingly
    #define MQTT_TELEMETRY_MS 300000    // Every 5 min
    #define MQTT_SAVE_DELAY_MS 5000     // Commands from Home Assistant are saved once they stop changing
    #if defined(ESP8266)
      #define MQTT_BUFFER_SIZE 1024     // Largest incoming message (config JSON)
    #else
      #define MQTT_BUFFER_SIZE 2048
    #endif
#endif

#ifdef LED
    // LED layouts, selectable at /led. Values are persisted (configuration.ledLayout) - append only.
    #define LED_LAYOUT_SINGLE 0   // One strip on ledPin
    #define LED_LAYOUT_DUAL   1   // Two strips on ledPin + ledPin2, rendered end to end or mirrored
    #define LED_LAYOUT_RING   2   // One chain on ledPin: outer ring (ledRingOuterSize) followed by inner ring
    #define LED_LAYOUT_COUNT  3
    #define LED_LAYOUT_BIT(layout) (1 << (layout))
    #define LED_LAYOUTS_ALL   0xFF

    // Hardware defaults, used on factory reset and when a configuration saved by older
    // firmware (which had these compiled in) is loaded. All of them are editable at /led.
    #define LED_CHANGE_MODE_SEC   0
    // LED_PIN_LIST is every GPIO offered as a data pin; on ESP8266 each one is a FastLED template instance
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
      #define LED_PIN GPIO_NUM_2
      #define LED_PIN_2 GPIO_NUM_4
      #define LED_PIN_LIST(X) X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(20) X(21)
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
      #define LED_PIN GPIO_NUM_2
      #define LED_PIN_2 GPIO_NUM_4
      #define LED_PIN_LIST(X) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(21) \
                              X(38) X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48)
    #elif defined(SEEED_XIAO_M0)
      #define LED_PIN 10
      #define LED_PIN_2 9
      #define LED_PIN_LIST(X) X(9) X(10)
    #elif defined(CONFIG_IDF_TARGET_ESP32)
      #define LED_PIN GPIO_NUM_12
      #define LED_PIN_2 GPIO_NUM_13
      #define LED_PIN_LIST(X) X(2) X(4) X(5) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(21) X(22) X(23) X(25) X(26) X(27) X(32) X(33)
    #elif defined(ESP8266)
      #define LED_PIN D3
      #define LED_PIN_2 D2
      // GPIO numbers (D3 = 0, D4 = 2, RX = 3, D2 = 4, D1 = 5, D6 = 12, D7 = 13, D5 = 14, D8 = 15)
      #define LED_PIN_LIST(X) X(0) X(2) X(3) X(4) X(5) X(12) X(13) X(14) X(15)
    #else
      #define LED_PIN 1
      #define LED_PIN_2 2
      #define LED_PIN_LIST(X) X(1) X(2)
    #endif
    #define LED_LAYOUT LED_LAYOUT_SINGLE
    #define LED_STRIP_SIZE 128        // Single strip, first strip of a dual layout, or total LEDs of a ring light
    #define LED_STRIP_SIZE_2 128      // Second strip of a dual layout
    #define LED_RING_OUTER_SIZE 141   // Ring light: LEDs in the outer ring (the original RingLight is 141 outer + 126 inner)
    #define LED_MAX_COUNT 1024        // Upper bound on physical LEDs across all strips
    #define LED_BRIGHTNESS 1  // 0-1, 1-max brightness, make sure your LEDs are powered accordingly
    #define LED_COLOR_ORDER GRB
#endif

#ifdef BUTTONS
  #define BUTTON_1_PIN     GPIO_NUM_0
  #define BUTTON_2_PIN     GPIO_NUM_1
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  #ifdef OLED // ESD32C3 has a built-in OLED ie ESP32C4 Dev board
    #define OLED_SCREEN_WIDTH 72 // OLED display width, in pixels
    #define OLED_SCREEN_HEIGHT 40 // OLED display height, in pixel
    #define OLED_I2C_ID  0x3C
  #endif
#endif


#ifdef LED
  // Custom modes defined at /modes. Effect values are persisted - append only.
  #define CUSTOM_MODE_COUNT       8     // Slots, registered after the built-in modes
  #define CUSTOM_MODE_NAME_SIZE   24
  #define CUSTOM_MODE_MAX_COLORS  8
  #define CUSTOM_MODE_MAX_SPEED   10
  #define CUSTOM_MODE_MAX_REPEAT  10
  #define CUSTOM_MODE_SMOOTH      0x01  // flags: blend between colors instead of hard edges
  #define CUSTOM_MODE_RING_OUTER  0x02  // flags: ring layout, draw on the outer ring only (effects with ringSelect)
  #define CUSTOM_MODE_RING_INNER  0x04  // flags: ring layout, inner ring only; neither bit means both rings

  #define CUSTOM_EFFECT_NONE      0     // Empty slot
  #define CUSTOM_EFFECT_FADE      1     // Whole light fades through the colors
  #define CUSTOM_EFFECT_SCROLL    2     // Colors travel from one end to the other
  #define CUSTOM_EFFECT_CENTER    3     // Colors flow from the center to both ends, or back
  #define CUSTOM_EFFECT_TWINKLE   4     // Random LEDs light up and fade out
  #define CUSTOM_EFFECT_CIRCLE    5     // Ring: colors rotate around each ring
  #define CUSTOM_EFFECT_HALVES    6     // Ring: each ring mirrored around its first LED
  #define CUSTOM_EFFECT_RINGS     7     // Ring: each ring one color, colors pass between rings
  #define CUSTOM_EFFECT_STARS     8     // Stars race from random LEDs to an end, trailing the other colors
  #define CUSTOM_EFFECT_FIRE      9     // Flickering flame (Fire2012), heat mapped onto the colors
  #define CUSTOM_EFFECT_BREATHE   10    // All LEDs fade in and out, next color with each breath
  #define CUSTOM_EFFECT_CHASE     11    // Theater chase: spaced lit LEDs marching along
  #define CUSTOM_EFFECT_SCANNER   12    // Larson scanner: an eye sweeping back and forth with a tail
  #define CUSTOM_EFFECT_WIPE      13    // Each color in turn fills the LEDs
  #define CUSTOM_EFFECT_LIGHTNING 14    // Random bursts of flashes on parts of the LEDs
  #define CUSTOM_EFFECT_COUNT     15

  struct custom_mode_t {
    char name[CUSTOM_MODE_NAME_SIZE];
    uint8_t effect;       // CUSTOM_EFFECT_*
    uint8_t colorCount;   // 1..CUSTOM_MODE_MAX_COLORS
    uint8_t flags;        // CUSTOM_MODE_*
    uint8_t direction;    // Effect specific, see CUSTOM_EFFECTS
    uint8_t speed;        // 1..CUSTOM_MODE_MAX_SPEED
    uint8_t repeat;       // Effect specific amount (palette repeats, stars, flame height...), 1..CUSTOM_MODE_MAX_REPEAT
    uint8_t colors[CUSTOM_MODE_MAX_COLORS][3];
  };
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

    // Fields added after the first release live past _loaded so existing configurations keep
    // their offsets. Each block carries its own marker and gets defaults when it isn't set.
    #ifdef LED
        uint8_t ledLayout;          // LED_LAYOUT_*
        uint8_t ledPin;             // GPIO of the first (or only) strip
        uint8_t ledPin2;            // GPIO of the second strip, dual layout only
        uint8_t ledColorOrder;      // FastLED EOrder value (RGB, GRB, ...)
        uint16_t ledStripSize2;     // LEDs on the second strip, dual layout without mirroring
        uint16_t ledRingOuterSize;  // LEDs in the outer ring, ring layout only
        uint8_t ledMirror;          // Dual layout: second strip repeats the first
        char _ledLoaded[4];         // "led" once the block above is initialized

        custom_mode_t customModes[CUSTOM_MODE_COUNT];
        char _customModesLoaded[4]; // "cm1" once customModes is initialized
    #endif

    #ifdef LED
        uint8_t ledPower;           // LEDs on (1) or off (0), from the main page, /api or Home Assistant
    #endif
    #ifdef WIFI
        char mqttServer[128];       // Blank disables MQTT
        uint16_t mqttPort;
        char mqttUser[64];          // Blank connects anonymously
        char mqttPassword[64];
        char mqttTopic[64];         // Base topic, the device id is appended
        uint8_t mqttDiscovery;      // Publish Home Assistant discovery
    #endif
    char _mqttLoaded[4];            // "mq1" once the block above is initialized
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

String CONFIG_getChipModel();
uint8_t CONFIG_getChipRevision();
uint32_t CONFIG_getFlashChipSize();

void intLEDOn();
void intLEDOff();
void intLEDBlink(uint16_t ms);

#ifdef LED
    float CONFIG_getLedBrightness(bool force = false);

    struct led_option_t {
      uint8_t value;
      const char *name;
    };

    extern const char * const LED_TYPE_NAMES[];     // Indexed by configuration.ledType
    extern const uint8_t LED_TYPE_COUNT;
    extern const led_option_t LED_COLOR_ORDERS[];
    extern const uint8_t LED_COLOR_ORDER_COUNT;
    extern const char * const LED_LAYOUT_IDS[];     // Indexed by LED_LAYOUT_*, used in JSON
    extern const char * const LED_LAYOUT_LABELS[];  // Indexed by LED_LAYOUT_*, used in the web UI
    extern const uint8_t LED_PINS[];
    extern const uint8_t LED_PIN_COUNT;

    const char* CONFIG_ledColorOrderName(uint8_t order);
    int CONFIG_ledColorOrderFromName(const char *name);  // -1 if unknown
    int CONFIG_ledLayoutFromName(const char *name);      // -1 if unknown
    bool CONFIG_isValidLedPin(uint8_t pin);
    uint16_t CONFIG_getLedCount(const configuration_t &c);  // Physical LEDs across all strips
    const char* CONFIG_checkLedHardware(const configuration_t &c);  // nullptr if valid, else the reason
    bool CONFIG_ledHardwareEquals(const configuration_t &a, const configuration_t &b);  // Same strips, i.e. no reboot needed
#endif
