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
  if (_display) {
    delete _display;
  }
#endif
  Log.noticeln(F("Device destroyed"));
}

void CDevice::loop() {
/*
  #ifdef OLED
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  // Alternate between SSID and IP every 3 seconds when WiFi is connected
  if (_display && wifiConnected && strlen(wifiSSID) > 0 && strlen(wifiIP) > 0) {
    if (millis() - tMillisDisplayToggle > 3000) {
      tMillisDisplayToggle = millis();
      displayToggleState = !displayToggleState;
      
      _display->clearDisplay();
      _display->setCursor(0, 0);
      
      if (displayToggleState) {
        _display->println("SSID:");
        _display->println(wifiSSID);
      } else {
        _display->println("IP:");
        _display->println(wifiIP);
      }
      
      _display->display();
    }
  }
  #endif
  #endif
*/

  _display->clearDisplay();

  //_display->drawRect(28, 24, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
  _display->drawRect(28, 24, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);

  _display->display();

}
