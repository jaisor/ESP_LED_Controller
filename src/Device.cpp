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

static const char* deviceStateToString(DeviceState state) {
  switch (state) {
    case DeviceState::INITIALIZING:    return "INITIALIZING";
    case DeviceState::WIFI_AP_CREATED:  return "WIFI_AP_CREATED";
    case DeviceState::WIFI_CONNECTED:   return "WIFI_CONNECTED";
    case DeviceState::WIFI_OFFLINE:     return "WIFI_OFFLINE";
    default:                           return "UNKNOWN";
  }
}

CDevice::CDevice() {

  _state = DeviceState::INITIALIZING;
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
    virtualCanvas->print("Starting up");
    
    // Initialize scrolling
    scrollOffset = 0;
    scrollDirection = 1; // 1 = right, -1 = left
    lastScrollTime = millis();
    scrollPaused = true; // Start with a pause
    pauseStartTime = millis();
    contentWidth = 0;
    scrollingEnabled = false;
    lastTimeUpdate = 0; // Force initial time update
    showingTempMessage = false;
    tempMessageEndTime = 0;
    
    // Boot progress
    bootProgress = 0;

    // WiFi connected banner
    wifiConnectedShownAt = 0;
    wifiConnectedBannerDone = false;
    
    // Calculate content bounds
    updateContentBounds();
    
    tMillisDisplayToggle = millis();
    displayToggleState = false;
    wifiConnected = false;
    strcpy(wifiSSID, "");
    strcpy(wifiPass, "");
    strcpy(wifiIP, "");
  #endif


  Log.infoln(F("Device initialized"));
}

void CDevice::setState(DeviceState state) {
  if (_state != state) {
    Log.infoln("Device state: %s -> %s", deviceStateToString(_state), deviceStateToString(state));
    _state = state;
    #ifdef OLED
    if (state == DeviceState::WIFI_CONNECTED) {
      wifiConnectedShownAt = millis();
      wifiConnectedBannerDone = false;
      lastTimeUpdate = 0; // Force time update after banner
    }
    #endif
  }
}

void CDevice::setBootProgress(uint8_t percent) {
  #ifdef OLED
  if (percent > 100) percent = 100;
  bootProgress = percent;
  #endif
}

void CDevice::setWifiAPInfo(const char* ssid, const char* password, const char* ip) {
  #ifdef OLED
  strncpy(wifiSSID, ssid, sizeof(wifiSSID) - 1);
  wifiSSID[sizeof(wifiSSID) - 1] = '\0';
  strncpy(wifiPass, password, sizeof(wifiPass) - 1);
  wifiPass[sizeof(wifiPass) - 1] = '\0';
  strncpy(wifiIP, ip, sizeof(wifiIP) - 1);
  wifiIP[sizeof(wifiIP) - 1] = '\0';
  #endif
}

void CDevice::setWifiConnectedInfo(const char* ip) {
  #ifdef OLED
  strncpy(wifiIP, ip, sizeof(wifiIP) - 1);
  wifiIP[sizeof(wifiIP) - 1] = '\0';
  #endif
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

  // Check if temporary message has expired
  if (showingTempMessage && millis() >= tempMessageEndTime) {
    showingTempMessage = false;
    lastTimeUpdate = 0; // Force time update after temp message
  }

  // State-based rendering (only when not showing a temporary message)
  if (!showingTempMessage) {
    switch (_state) {
      case DeviceState::INITIALIZING:
        drawInitializing();
        break;
      case DeviceState::WIFI_AP_CREATED:
        drawWifiAP();
        break;
      case DeviceState::WIFI_CONNECTED:
        drawWifiConnected();
        break;
      default:
        break;
    }
  }
  
  // Update scroll position every 50ms
  if (millis() - lastScrollTime >= 50) {
    lastScrollTime = millis();
    
    // Only scroll if content extends beyond visible area
    if (scrollingEnabled) {
      // Check if we're in a pause state
      if (scrollPaused) {
        if (millis() - pauseStartTime >= SCROLL_PAUSE_MS) {
          scrollPaused = false; // Resume scrolling
        }
      } else {
        // Update scroll offset
        scrollOffset += scrollDirection;
        
        // Calculate max scroll based on actual content width
        int16_t maxScroll = contentWidth - OLED_SCREEN_WIDTH;
        if (maxScroll < 0) maxScroll = 0;
        
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

#ifdef OLED
void CDevice::updateContentBounds() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;
  
  // Scan virtual canvas to find rightmost occupied pixel
  contentWidth = 0;
  
  for (int16_t x = VIRTUAL_WIDTH - 1; x >= 0; x--) {
    bool columnHasContent = false;
    for (int16_t y = 0; y < VIRTUAL_HEIGHT; y++) {
      if (virtualCanvas->getPixel(x, y)) {
        columnHasContent = true;
        break;
      }
    }
    if (columnHasContent) {
      contentWidth = x + 1; // +1 because we want the width, not the index
      break;
    }
  }
  
  // Enable scrolling only if content extends beyond visible area (72 pixels)
  scrollingEnabled = (contentWidth > OLED_SCREEN_WIDTH);
  
  // Reset scroll position if scrolling is disabled
  if (!scrollingEnabled) {
    scrollOffset = 0;
  }
  
  Log.infoln("Content width: %d, scrolling: %s", contentWidth, scrollingEnabled ? "enabled" : "disabled");
  #endif
}

void CDevice::displayTemporaryMessage(const char* message, unsigned long durationMs) {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;
  
  // Clear and draw message
  virtualCanvas->fillScreen(0);
  virtualCanvas->setTextColor(1);
  virtualCanvas->setTextSize(2);
  
  virtualCanvas->setCursor(0, 10);
  virtualCanvas->print(message);
  
  // Update content bounds
  updateContentBounds();
  
  // Set temp message tracking
  showingTempMessage = true;
  tempMessageEndTime = millis() + durationMs;
  #endif
}

void CDevice::drawInitializing() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;

  virtualCanvas->fillScreen(0);
  virtualCanvas->setTextColor(1);
  virtualCanvas->setTextSize(1); // 6x8 font

  // "Starting up" centered
  const char* msg = "Starting up";
  int16_t textWidth = strlen(msg) * 6;
  virtualCanvas->setCursor((OLED_SCREEN_WIDTH - textWidth) / 2, 4);
  virtualCanvas->print(msg);

  // Progress bar: 4px padding each side, 2px below text
  int16_t barX = 4;
  int16_t barY = 18;
  int16_t barW = OLED_SCREEN_WIDTH - 8;
  int16_t barH = 10;

  // Outline
  virtualCanvas->drawRect(barX, barY, barW, barH, 1);

  // Filled portion
  int16_t fillW = (int16_t)((barW - 2) * bootProgress / 100);
  if (fillW > 0) {
    virtualCanvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, 1);
  }

  updateContentBounds();
  #endif
}

void CDevice::drawWifiAP() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;

  virtualCanvas->fillScreen(0);
  virtualCanvas->setTextColor(1);
  virtualCanvas->setTextSize(1); // Smallest font (6x8)

  // Line 1: "WiFi Created"
  

  // Line 2: SSID
  virtualCanvas->setCursor(0, 9);
  virtualCanvas->print(wifiSSID);

  // Line 3: Password
  virtualCanvas->setCursor(0, 18);
  virtualCanvas->print(wifiPass);

  // Line 4: IP address
  virtualCanvas->setCursor(0, 27);
  virtualCanvas->print(wifiIP);

  updateContentBounds();
  #endif
}

void CDevice::drawWifiConnected() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;

  // Show "WiFi connected" + IP for 10 seconds, then switch to time
  if (!wifiConnectedBannerDone && millis() - wifiConnectedShownAt < 10000) {
    virtualCanvas->fillScreen(0);
    virtualCanvas->setTextColor(1);
    virtualCanvas->setTextSize(1);

    const char* msg = "WiFi connected";
    virtualCanvas->setCursor(0, 0);
    virtualCanvas->print(msg);

    virtualCanvas->setCursor(0, 9);
    virtualCanvas->print(wifiIP);

    virtualCanvas->setCursor(0, 18);
    virtualCanvas->print("Web server ready");

    updateContentBounds();
  } else {
    if (!wifiConnectedBannerDone) {
      wifiConnectedBannerDone = true;
      lastTimeUpdate = 0; // Force immediate time draw
    }
    drawTime();
  }
  #endif
}

void CDevice::drawTime() {
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  if (!virtualCanvas) return;

  // Update once per minute
  if (millis() - lastTimeUpdate < 60000) return;
  lastTimeUpdate = millis();

  virtualCanvas->fillScreen(0);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    virtualCanvas->setTextColor(1);
    virtualCanvas->setTextSize(2); // Large font for time

    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;

    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour12, timeinfo.tm_min);
    int16_t textWidth = strlen(timeStr) * 12; // textSize 2 = 12px per char

    virtualCanvas->setCursor((OLED_SCREEN_WIDTH - textWidth) / 2, 0);
    virtualCanvas->print(timeStr);

    // AM/PM below
    const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
    int16_t ampmWidth = strlen(ampm) * 12;
    virtualCanvas->setCursor((OLED_SCREEN_WIDTH - ampmWidth) / 2, 20);
    virtualCanvas->print(ampm);
  } else {
    virtualCanvas->setTextColor(1);
    virtualCanvas->setTextSize(1);
    virtualCanvas->setCursor(0, 10);
    virtualCanvas->print("Time N/A");
  }

  updateContentBounds();
  #endif
}
#endif
