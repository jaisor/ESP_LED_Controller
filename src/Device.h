#pragma once

#include <functional>
#include <deque>
#include "Configuration.h"

#ifdef OLED
  #include <Adafruit_SSD1306.h>
  #include <Adafruit_GFX.h>
#endif


#define STALE_READING_AGE_MS 10000 // 10 sec

enum class DeviceState : uint8_t {
  INITIALIZING = 0,
  WIFI_AP_CREATED,
  WIFI_CONNECTED,
  WIFI_OFFLINE
};

class CDevice {

public:
	CDevice();
  virtual ~CDevice();
  void loop();

  DeviceState getState() const { return _state; }
  void setState(DeviceState state);
  void setBootProgress(uint8_t percent); // 0-100

  // Set WiFi info for OLED display (call before setState)
  void setWifiAPInfo(const char* ssid, const char* password, const char* ip);
  void setWifiConnectedInfo(const char* ip);

  #ifdef OLED
  Adafruit_SSD1306 *_display;
  GFXcanvas1 *virtualCanvas;
  
  void updateContentBounds();
  void displayTemporaryMessage(const char* message, unsigned long durationMs);
  #endif

private:
  DeviceState _state;
  unsigned long tMillisUp;
  unsigned long minDelayMs;

  #ifdef OLED
    unsigned long tMillisDisplayToggle;
    bool displayToggleState;
    char wifiSSID[64];
    char wifiPass[64];
    char wifiIP[32];
    bool wifiConnected;
    
    // Boot progress bar
    uint8_t bootProgress; // 0-100

    // WiFi connected display timing
    unsigned long wifiConnectedShownAt;
    bool wifiConnectedBannerDone;
    
    // Virtual screen scrolling
    int16_t scrollOffset;
    int8_t scrollDirection;
    unsigned long lastScrollTime;
    bool scrollPaused;
    unsigned long pauseStartTime;
    int16_t contentWidth; // Rightmost occupied pixel
    bool scrollingEnabled;
    unsigned long lastTimeUpdate; // Track when time was last updated
    bool showingTempMessage;
    unsigned long tempMessageEndTime;
    static const int16_t VIRTUAL_WIDTH = 128;
    static const int16_t VIRTUAL_HEIGHT = 40;
    static const int16_t HARDWARE_X_OFFSET = 28;
    static const int16_t HARDWARE_Y_OFFSET = 24;
    static const unsigned long SCROLL_PAUSE_MS = 500;

    void drawInitializing();
    void drawWifiAP();
    void drawWifiConnected();
    void drawTime();
  #endif
};
