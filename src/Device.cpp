#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>

#include "Device.h"

#if defined(OLED)
  #include <U8x8lib.h>
  #include <Wire.h>
  #ifdef U8X8_HAVE_HW_SPI
    #include <SPI.h>
  #endif
#endif

int width = 72;
int height = 40;
int xOffset = 28; // = (132-w)/2
int yOffset = 24; // = (64-h)/2

CDevice::CDevice() {

  tMillisUp = millis();
/*
  #ifdef CONFIG_IDF_TARGET_ESP32C3
    // ESP32C3 uses GPIO 6,7 for SDA,SCL - see https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
    if (Wire.begin(GPIO_NUM_5, GPIO_NUM_6)) {
      Log.errorln(F("ESP32C3 I2C Wire initialization failed on pins SDA:%d, SCL:%d"), GPIO_NUM_5, GPIO_NUM_6);
    };
    Wire.setBufferSize(73+5);
    delay(1000);
  #endif

  #ifdef OLED
    _display = new Adafruit_SSD1306(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, -1);
    if(!_display->begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ID)) {
        Log.errorln("SSD1306 OLED initialiation failed with ID %x", OLED_I2C_ID);
        while (1);
    }
    _display->clearDisplay();
    _display->setTextColor(WHITE);
    _display->setTextSize(0);
  #endif
*/

  #ifdef CONFIG_IDF_TARGET_ESP32C3
    u8 = new U8X8_SSD1306_72X40_ER_HW_I2C(U8X8_PIN_NONE, GPIO_NUM_6, GPIO_NUM_5);
    delay(1000);
    u8->begin();
    u8->setContrast(255); // set contrast to maximum
    u8->setBusClock(400000); //400kHz I2C
    u8->setFont(u8x8_font_chroma48medium8_r);
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
  delete u8;
#endif
  Log.noticeln(F("Device destroyed"));
}

void CDevice::loop() {

  #ifdef OLED
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  // Alternate between SSID and IP every 3 seconds when WiFi is connected
  if (wifiConnected && strlen(wifiSSID) > 0 && strlen(wifiIP) > 0) {
    if (millis() - tMillisDisplayToggle > 3000) {
      tMillisDisplayToggle = millis();
      displayToggleState = !displayToggleState;
      
      u8->clearDisplay();
      if (displayToggleState) {
        u8->drawString(0, 0, "SSID:");
        // Wrap SSID to next line if longer than 9 characters
        if (strlen(wifiSSID) > 9) {
          char line1[10];
          strncpy(line1, wifiSSID, 9);
          line1[9] = '\0';
          u8->drawString(0, 1, line1);
          u8->drawString(0, 2, wifiSSID + 9);
        } else {
          u8->drawString(0, 1, wifiSSID);
        }
      } else {
        u8->drawString(0, 0, "IP:");
        // Wrap IP to next line if longer than 9 characters
        if (strlen(wifiIP) > 9) {
          char line1[10];
          strncpy(line1, wifiIP, 9);
          line1[9] = '\0';
          u8->drawString(0, 1, line1);
          u8->drawString(0, 2, wifiIP + 9);
        } else {
          u8->drawString(0, 1, wifiIP);
        }
      }
    }
  }
  #endif
  #endif

}

#ifdef OLED
void CDevice::displayMessage(const char* line1, const char* line2) {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (u8) {
    wifiConnected = false;
    u8->clearDisplay();
    u8->drawString(0, 0, line1);
    if (line2) {
      u8->drawString(0, 1, line2);
    }
  }
  #endif
}

void CDevice::displayWifiInfo(const char* ssid, const char* ip) {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (u8) {
    strncpy(wifiSSID, ssid, sizeof(wifiSSID) - 1);
    wifiSSID[sizeof(wifiSSID) - 1] = '\0';
    strncpy(wifiIP, ip, sizeof(wifiIP) - 1);
    wifiIP[sizeof(wifiIP) - 1] = '\0';
    wifiConnected = true;
    displayToggleState = true;
    tMillisDisplayToggle = millis();
    
    u8->clearDisplay();
    u8->drawString(0, 0, "SSID:");
    // Wrap SSID to next line if longer than 9 characters
    if (strlen(wifiSSID) > 9) {
      char line1[10];
      strncpy(line1, wifiSSID, 9);
      line1[9] = '\0';
      u8->drawString(0, 1, line1);
      u8->drawString(0, 2, wifiSSID + 9);
    } else {
      u8->drawString(0, 1, wifiSSID);
    }
  }
  #endif
}

void CDevice::clearDisplay() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (u8) {
    wifiConnected = false;
    u8->clearDisplay();
  }
  #endif
}
#endif
