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
#include <PubSubClient.h>
#include <Print.h>

#include "BaseManager.h"
#include "modes/BaseMode.h"

#include "Device.h"

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
  unsigned long tsAPReboot;

  // MQTT / Home Assistant (WifiManagerMQTT.cpp). The client is only used from loop(); web handlers
  // set the flags below, since PubSubClient isn't safe to call from the async web server's task.
  WiFiClient mqttClient;
  PubSubClient mqtt;
  String mqttBaseTopic;               // <mqttTopic>/<device id>
  unsigned long tsMqttConnect = 0;    // Last connection attempt, 0 to try right away
  unsigned long tsMqttTelemetry = 0;
  unsigned long tsMqttSave = 0;       // Last Home Assistant command, saved MQTT_SAVE_DELAY_MS later
  bool mqttSavePending = false;
  volatile bool mqttDiscoveryNeeded = false;  // Effect list changed
  bool mqttStatePublished = false;
  uint8_t mqttLastPower, mqttLastBrightness, mqttLastMode;
  volatile int mqttStateCode = MQTT_DISCONNECTED;  // mqtt.state() as of the last loop, for the web header

  std::vector<CBaseMode*> *modes;
  uint8_t ledLayout;  // Layout the running modes were set up for
  
  unsigned long lastModeChangeMs = 0;
  
  AsyncWebServer* server;

  JsonDocument configJson;
  JsonDocument deviceJson;

  String currentIP();
  void connect();
  void listen();

  void handleRoot(AsyncWebServerRequest *request);
  void handleWifi(AsyncWebServerRequest *request);

  void handleDevice(AsyncWebServerRequest *request);
  void handleLED(AsyncWebServerRequest *request);
  void handleModes(AsyncWebServerRequest *request);
  void handleCustomModeUpdate(AsyncWebServerRequest *request, JsonObject body);
  String customModesPageJson(int savedSlot = -1);
  int customModeIndex(uint8_t slot);
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

  bool isMqttConfigured();
  String mqttTopic(const char *suffix);
  void mqttLoop();
  void mqttConnect();
  void mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  void mqttHandleCommand(JsonDocument &command);
  void mqttHandleConfig(JsonDocument &json);
  void mqttPublishDiscovery();
  void mqttPublishState();
  void mqttPublishTelemetry();
  void mqttPublishJson(const String &topic, JsonDocument &doc, bool retain);
  void mqttEffectNames(std::vector<std::pair<uint8_t, String>> &names);
  String mqttStatusHtml();
  void handleMqttReconnect(AsyncWebServerRequest *request);

  // c: configuration holding the custom mode definitions, when checking one that isn't applied yet
  bool isModeSelectable(uint8_t index, uint8_t layout, const configuration_t &c = configuration);
  // allowHardware: also apply LED hardware fields, which only take effect after a save and reboot
  bool updateConfigFromJson(JsonDocument jsonObj, bool allowHardware, String *error = nullptr);

  CDevice *device;

public:
	CWifiManager();
  virtual void loop();

  virtual const bool isRebootNeeded() { return rebootNeeded; }
  virtual const bool isJobDone() { return !isApMode(); }

  void setModes(std::vector<CBaseMode*> *modes, uint8_t layout) { this->modes = modes; this->ledLayout = layout; }
  void updateModeChangeTime() { lastModeChangeMs = millis(); }

  void setDevice(CDevice* device) { this->device = device; };
};
