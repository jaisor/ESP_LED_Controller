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
  #endif
};
