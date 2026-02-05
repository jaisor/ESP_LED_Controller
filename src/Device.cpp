#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>

#include "Device.h"

#if defined(OLED)
  #include <Adafruit_SSD1306.h>
  #include <Adafruit_GFX.h>
  #include <Wire.h>
#endif

int width = 72;
int height = 40;
int xOffset = 28; // = (132-w)/2
int yOffset = 24; // = (64-h)/2

CDevice::CDevice() {

  tMillisUp = millis();

  #if defined(CONFIG_IDF_TARGET_ESP32C3) && defined(OLED)
    // ESP32C3 uses GPIO 5=SDA, GPIO 6=SCL - see https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
    Wire.begin(GPIO_NUM_5, GPIO_NUM_6);
    Wire.setClock(400000); // 400kHz I2C
    delay(100);
    
    _display = new Adafruit_SSD1306(128, 64, &Wire, -1);
    if(!_display->begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ID)) {
        Log.errorln("SSD1306 OLED initialization failed with ID %x", OLED_I2C_ID);
    } else {
        Log.infoln("OLED initialized successfully");
        _display->clearDisplay();
        _display->setTextColor(SSD1306_WHITE);
        _display->setTextSize(1);
        _display->display();
    }
    
    // Create virtual canvas (128x40)
    virtualCanvas = new GFXcanvas1(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
    virtualCanvas->fillScreen(0); // Clear to black
    virtualCanvas->setTextColor(1); // White text
    virtualCanvas->setTextSize(1);
    virtualCanvas->setCursor(0, 10);
    virtualCanvas->print("Wifi: --");
    virtualCanvas->setCursor(0, 20);
    virtualCanvas->print("IP: 192.168.10.10");
    virtualCanvas->setCursor(0, 30);
    virtualCanvas->print("Device initializing...");
    
    // Initialize scrolling
    scrollOffset = 0;
    scrollDirection = 1; // 1 = right, -1 = left
    lastScrollTime = millis();
    scrollPaused = true; // Start with a pause
    pauseStartTime = millis();
    
    tMillisDisplayToggle = millis();
    displayToggleState = false;
    wifiConnected = false;
    strcpy(wifiSSID, "");
    strcpy(wifiIP, "");
  #endif


  Log.infoln(F("Device initialized"));
}

CDevice::~CDevice() { 
#ifdef OLED
  if (virtualCanvas) {
    delete virtualCanvas;
  }
  if (_display) {
    delete _display;
  }
#endif
  Log.noticeln(F("Device destroyed"));
}

void CDevice::loop() {
  #ifdef OLED
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  
  // Update scroll position every 50ms
  if (millis() - lastScrollTime >= 50) {
    lastScrollTime = millis();
    
    // Check if we're in a pause state
    if (scrollPaused) {
      if (millis() - pauseStartTime >= SCROLL_PAUSE_MS) {
        scrollPaused = false; // Resume scrolling
      }
    } else {
      // Update scroll offset
      scrollOffset += scrollDirection;
      
      // Calculate max scroll range (virtual width - hardware width)
      int16_t maxScroll = VIRTUAL_WIDTH - OLED_SCREEN_WIDTH;
      
      // Check boundaries and start pause
      if (scrollOffset >= maxScroll) {
        scrollOffset = maxScroll;
        scrollDirection = -1;
        scrollPaused = true;
        pauseStartTime = millis();
      } else if (scrollOffset <= 0) {
        scrollOffset = 0;
        scrollDirection = 1;
        scrollPaused = true;
        pauseStartTime = millis();
      }
    }
    
    // Clear hardware display
    _display->clearDisplay();
    
    // Copy visible portion of virtual canvas to hardware display
    // The virtual canvas is 128x40, we're viewing a 72x40 window
    // Position it at 28,24 on the 128x64 hardware display
    for (int16_t y = 0; y < VIRTUAL_HEIGHT && y < OLED_SCREEN_HEIGHT; y++) {
      for (int16_t x = 0; x < OLED_SCREEN_WIDTH; x++) {
        // Get pixel from virtual canvas at scrolled position
        int16_t virtualX = x + scrollOffset;
        if (virtualX >= 0 && virtualX < VIRTUAL_WIDTH) {
          uint16_t pixel = virtualCanvas->getPixel(virtualX, y);
          if (pixel) {
            _display->drawPixel(HARDWARE_X_OFFSET + x, HARDWARE_Y_OFFSET + y, SSD1306_WHITE);
          }
        }
      }
    }
    
    // Draw border for reference
    //_display->drawRect(HARDWARE_X_OFFSET, HARDWARE_Y_OFFSET, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
    
    _display->display();
  }
  
  #endif
  #endif
}
