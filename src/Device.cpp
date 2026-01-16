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
    //u8->setFont(u8g2_font_ncenB10_tr);
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
  char st[256];
  
  //_display->setTextSize(0);
  //_display->setCursor(28,24);
  //_display->printf("Test");

  //u8->clearBuffer(); // clear the internal memory
  //u8->drawFrame(xOffset+0, yOffset+0, width, height); //draw a frame around the border
  //u8->setCursor(xOffset+15, yOffset+25);
  //u8->printf("%dx%d", width, height);
  u8->setFont(u8x8_font_chroma48medium8_r);
  u8->drawString(0,0,"HI Amazon");
  //u8->sendBuffer(); // transfer internal memory to the display

  #endif

}
