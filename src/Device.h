#pragma once

#include <functional>
#include <deque>
#include "Configuration.h"

#ifdef OLED
  #include <Adafruit_SSD1306.h>
  #include <Adafruit_GFX.h>
#endif


#define STALE_READING_AGE_MS 10000 // 10 sec

class CDevice {

public:
	CDevice();
  ~CDevice();
  void loop();

  #ifdef OLED
  //Adafruit_SSD1306* display() const { return _display; };
  Adafruit_SSD1306 *_display;
  GFXcanvas1 *virtualCanvas;
  #endif

private:
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
  static const int16_t VIRTUAL_WIDTH = 128;
  static const int16_t VIRTUAL_HEIGHT = 40;
  static const int16_t HARDWARE_X_OFFSET = 28;
  static const int16_t HARDWARE_Y_OFFSET = 24;
  static const unsigned long SCROLL_PAUSE_MS = 500;
  #endif
};
