#pragma once

#if defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Print.h>

#include "BaseManager.h"
#include "modes/BaseMode.h"

#ifdef OLED
  #include <Adafruit_SSD1306.h>
  #include <Adafruit_GFX.h>
#endif

typedef enum {
  WF_CONNECTING = 0,
  WF_LISTENING = 1
} wifi_status;

class CWifiManager: public CBaseManager {

private:
  bool rebootNeeded;
  uint8_t wifiRetries;
  unsigned long tMillis;
  wifi_status status;
  char softAP_SSID[32];
  char SSID[32];
  char mqttSubcribeTopicConfig[255];
  unsigned long tsAPReboot;

  std::vector<CBaseMode*> *modes;
  
  unsigned long lastModeChangeMs = 0;
  
  AsyncWebServer* server;

  JsonDocument configJson;
  JsonDocument deviceJson;

  void connect();
  void listen();

  void handleRoot(AsyncWebServerRequest *request);
  void handleWifi(AsyncWebServerRequest *request);

  void handleDevice(AsyncWebServerRequest *request);
  void handleFactoryReset(AsyncWebServerRequest *request);
  void handleReboot(AsyncWebServerRequest *request);
  void handleStyleCSS(AsyncWebServerRequest *request);
  //
  void handleRestAPI_LED(AsyncWebServerRequest *request);
  void handleRestAPI_Device(AsyncWebServerRequest *request);
  void handleRestAPI_Config(AsyncWebServerRequest *request);

  void printHTMLTop(Print *p);
  void printHTMLBottom(Print *p);
  void printHTMLMain(Print *p);

  bool isApMode();

  bool updateConfigFromJson(JsonDocument jsonObj);

#ifdef OLED
  Adafruit_SSD1306 *display;
#endif

public:
	CWifiManager();
  virtual void loop();

  virtual const bool isRebootNeeded() { return rebootNeeded; }
  virtual const bool isJobDone() { return !isApMode(); }

  void setModes(std::vector<CBaseMode*> *modes) { this->modes = modes; }
  void updateModeChangeTime() { lastModeChangeMs = millis(); }

#ifdef OLED
  void setDisplay(Adafruit_SSD1306* display) { this->display = display; };
#endif
};
