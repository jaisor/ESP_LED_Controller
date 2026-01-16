#pragma once

#include <functional>
#include <deque>
#include "Configuration.h"

#ifdef OLED
  //#include <Adafruit_SSD1306.h>
  //#include <Adafruit_GFX.h>
  #include <U8x8lib.h>
#endif


#define STALE_READING_AGE_MS 10000 // 10 sec

class CDevice {

public:
	CDevice();
  ~CDevice();
  void loop();

  #ifdef OLED
  //Adafruit_SSD1306* display() const { return _display; };
  //Adafruit_SSD1306 *_display;

  U8X8_SSD1306_72X40_ER_HW_I2C *u8;

  #endif

private:
  unsigned long tMillisUp;
  unsigned long minDelayMs;
};
