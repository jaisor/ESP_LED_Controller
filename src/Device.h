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
    char wifiSSID[32];
    char wifiIP[32];
    bool wifiConnected;
    
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
  #endif
};
