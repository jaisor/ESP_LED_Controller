#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 or ESP32 platform!
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <time.h>
#include <ezTime.h>
#include <ElegantOTA.h>
#include <StreamUtils.h>
#include <AsyncJson.h>
#include <version.h>
#include "Configuration.h"
#include "wifi/WifiManager.h"
#include "wifi/HTMLAssets.h"

#define MAX_CONNECT_TIMEOUT_MS 15000 // 10 seconds to connect before creating its own AP
#define POST_UPDATE_INTERVAL 300000 // Every 5 min

const int RSSI_MAX =-50;// define maximum straighten of signal in dBm
const int RSSI_MIN =-100;// define minimum strength of signal in dBm

WiFiClient espClient;

const unsigned char icon_wifi [] PROGMEM = {
	0x00, 0x00, 0x70, 0x00, 0x7e, 0x00, 0x07, 0x80, 0x01, 0xc0, 0x70, 0xe0, 0x7c, 0x30, 0x0e, 0x38, 
	0x03, 0x18, 0x61, 0x8c, 0x78, 0xcc, 0x1c, 0xc4, 0x0c, 0x66, 0x46, 0x66, 0x66, 0x66, 0x00, 0x00
};

const unsigned char icon_ip [] PROGMEM = {
	0x0, 0xee, 0x49, 0x49, 0x4e, 0x48, 0xe8, 0x0
};

int dBmtoPercentage(int dBm) {
  int quality;
  if(dBm <= RSSI_MIN) {
    quality = 0;
  } else if(dBm >= RSSI_MAX) {  
    quality = 100;
  } else {
    quality = 2 * (dBm + 100);
  }
  return quality;
}

CWifiManager::CWifiManager()
:rebootNeeded(false), wifiRetries(0) {

  deviceJson["dev_name"] = configuration.name;
  deviceJson["version"] = VERSION;
  deviceJson["version_short"] = VERSION_SHORT;

  strcpy(SSID, configuration.wifiSsid);
  server = new AsyncWebServer(WEB_SERVER_PORT);
  connect();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  uint32_t deviceId = CONFIG_getDeviceId();
  deviceJson["device_id"] = deviceId;
  Log.infoln("Device ID: '%i'", deviceId);

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s' with power %i", SSID, configuration.wifiPower);
    WiFi.begin(SSID, configuration.wifiPassword);
#if defined(ESP32)
    wifi_power_t txPower = WIFI_POWER_19_5dBm; // default
    switch (configuration.wifiPower) {
      case 76: txPower = WIFI_POWER_19dBm; break;   // 19dBm
      case 74: txPower = WIFI_POWER_18_5dBm; break; // 18.5dBm
      case 68: txPower = WIFI_POWER_17dBm; break;   // 17dBm
      case 60: txPower = WIFI_POWER_15dBm; break;   // 15dBm
      case 52: txPower = WIFI_POWER_13dBm; break;   // 13dBm
      case 44: txPower = WIFI_POWER_11dBm; break;   // 11dBm
      case 34: txPower = WIFI_POWER_8_5dBm; break;  // 8.5dBm
      case 28: txPower = WIFI_POWER_7dBm; break;    // 7dBm
      case 20: txPower = WIFI_POWER_5dBm; break;    // 5dBm
      case 8:  txPower = WIFI_POWER_2dBm; break;    // 2dBm
      case -4: txPower = WIFI_POWER_MINUS_1dBm; break; // -1dBm
      default: txPower = WIFI_POWER_19_5dBm; // 19.5dBm
    }
    WiFi.setTxPower(txPower);
#elif defined(ESP8266)
    float txPower = 20.5; // default 
    switch (configuration.wifiPower) {
      case 76: txPower = 19; break;   // 19dBm
      case 74: txPower = 18.5; break; // 18.5dBm
      case 68: txPower = 17; break;   // 17dBm
      case 60: txPower = 15; break;   // 15dBm
      case 52: txPower = 13; break;   // 13dBm
      case 44: txPower = 11; break;   // 11dBm
      case 34: txPower = 8.5; break;  // 8.5dBm
      case 28: txPower = 7; break;    // 7dBm
      case 20: txPower = 5; break;    // 5dBm
      case 8:  txPower = 2; break;    // 2dBm
      case -4: txPower = 1; break; // -1dBm
      default: txPower = 20.5; // 19.5dBm
    }
    WiFi.setOutputPower(txPower); 
#endif
    wifiRetries = 0;

  } else {

    // Create AP using fallback and chip ID
    sprintf_P(softAP_SSID, "%s_%i", WIFI_FALLBACK_SSID, deviceId);
    Log.infoln("Creating WiFi: '%s' / '%s'", softAP_SSID, WIFI_FALLBACK_PASS);

    if (WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS)) {
      wifiRetries = 0;
      tsAPReboot = millis();
      Log.infoln("Wifi AP '%s' created, listening on '%s'", softAP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
      Log.errorln("Wifi AP faliled");
    };

  }
  
}

void CWifiManager::listen() {

  status = WF_LISTENING;
  // Web
  server->on("/", std::bind(&CWifiManager::handleRoot, this, std::placeholders::_1));
  server->on("/style.css", HTTP_GET, std::bind(&CWifiManager::handleStyleCSS, this, std::placeholders::_1));
  server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(404); });
  //

  server->on("/wifi", HTTP_GET | HTTP_POST, std::bind(&CWifiManager::handleWifi, this, std::placeholders::_1));
#ifdef TEMP_SENSOR
  server->on("/sensor", HTTP_GET | HTTP_POST, std::bind(&CWifiManager::handleSensor, this, std::placeholders::_1));
#endif
  server->on("/device", HTTP_GET | HTTP_POST, std::bind(&CWifiManager::handleDevice, this, std::placeholders::_1));
  //
  server->on("/factory_reset", HTTP_POST, std::bind(&CWifiManager::handleFactoryReset, this, std::placeholders::_1));
  server->on("/reboot", HTTP_POST, std::bind(&CWifiManager::handleReboot, this, std::placeholders::_1));
#ifdef WEB_LOGGING
  server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log.traceln("handleLog");
    intLEDOn();
    AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8");
    response->println(logStream.str().c_str());
    request->send(response);
    intLEDOff();
  });
#endif
  server->on("/config", HTTP_GET | HTTP_POST, std::bind(&CWifiManager::handleRestAPI_Config, this, std::placeholders::_1));
  server->on("/api", HTTP_GET, std::bind(&CWifiManager::handleRestAPI_LED, this, std::placeholders::_1));
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    bool success = this->updateConfigFromJson(json.as<JsonObject>());
    if (success) {
      handleRestAPI_LED(request);
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8");
      response->print("ERROR");
      response->setCode(500);
      request->send(response);
    }
  });
  server->addHandler(handler);

  server->begin();
  Log.infoln("Web server listening on %s port %i", WiFi.localIP().toString().c_str(), WEB_SERVER_PORT);

  deviceJson["ip"] = WiFi.localIP().toString();
  
  // NTP
  Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);
  configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Log.noticeln("The time is %i:%i", timeinfo.tm_hour,timeinfo.tm_min);
  }

  // OTA
  ElegantOTA.begin(server);
}

void CWifiManager::loop() {

  ElegantOTA.loop();

  if (rebootNeeded && millis() - tMillis > 300) {
    Log.noticeln("Rebooting...");
  #if defined(ESP32)
    ESP.restart();
  #elif defined(ESP8266)
    ESP.reset();
  #endif
    return;
  }

  if (WiFi.status() == WL_CONNECTED || isApMode() ) {
    // WiFi is connected

    if (status != WF_LISTENING) {  
      // Start listening for requests
      listen();
      return;
    }

    #ifdef OLED
    display->setTextSize(0);
    display->drawBitmap(0, 0, icon_wifi, 16, 16, 1);
    display->drawBitmap(18, 8, icon_ip, 8, 8, 1);
    display->setCursor(18,0);
    if (isApMode()) {
      display->print(WIFI_FALLBACK_SSID);
      display->print("...");
      display->setCursor(26,8);
      display->print(WiFi.softAPIP().toString().c_str());  
    } else {
      display->print(configuration.wifiSsid);
      display->print(" ");
      display->print(dBmtoPercentage(WiFi.RSSI()));
      display->print("%");
      display->setCursor(26,8);
      display->print(WiFi.localIP().toString());  
    }
    
    display->display();
    #endif

    if (isApMode() && strlen(configuration.wifiSsid)) {
      if (WiFi.softAPgetStationNum() > 0)  {
        tsAPReboot = millis();
      } else if (millis() - tsAPReboot > 60000) {
        // Reboot if in AP mode and no connected clients, in hope of connecting to real AP
        Log.infoln(F("Rebooting after a minute in AP with no connections"));
        rebootNeeded = true;
      }
    }

  } else if (WiFi.status() == WL_NO_SSID_AVAIL && !isApMode()) {
    // Can't find desired AP
    if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
      tMillis = millis();
      if (++wifiRetries > 1) {
        Log.warningln("Failed to find previous AP (wifi status %i) after %l ms, create an AP instead", WiFi.status(), (millis() - tMillis));
        strcpy(SSID, "");
        WiFi.disconnect(false, true);
        connect();
      } else {
        Log.warningln("Can't find previous AP (wifi status %i) trying again attempt: %i", WiFi.status(), wifiRetries);
      }
      //Log.infoln("WifiMode == %i", WiFi.getMode());
    }
  } else {
    // WiFi is down
    switch (status) {
      case WF_LISTENING: {
      Log.infoln("Disconnecting %i", status);
      server->end();
      status = WF_CONNECTING;
      connect();
      } break;
      case WF_CONNECTING: {
        if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
          tMillis = millis();
          if (++wifiRetries > 3) {
            Log.warningln("Connecting failed (wifi status %i) after %l ms, create an AP instead", WiFi.status(), (millis() - tMillis));
            strcpy(SSID, "");
          }
          connect();
        }
      } break;
    } // switch
  }
  
}

void CWifiManager::handleRoot(AsyncWebServerRequest *request) {
  Log.traceln("handleRoot");
  intLEDOn();

  AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");
  printHTMLTop(response);
  printHTMLMain(response);
  printHTMLBottom(response);
  request->send(response);

  intLEDOff();
}

void CWifiManager::handleWifi(AsyncWebServerRequest *request) {
  Log.traceln("handleWifi: %i - %s", request->method(), request->methodToString());
  intLEDOn();

  if (request->method() == HTTP_POST) {
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    String wifiPowerStr = request->arg("wifiPower");
    int wifiPower = wifiPowerStr.length() > 0 ? wifiPowerStr.toInt() : 78;

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");

    printHTMLTop(response);
    response->printf("<p>Connecting to '%s' ... see you on the other side!</p>", ssid.c_str());
    printHTMLBottom(response);

    request->send(response);

    ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
    password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));
    configuration.wifiPower = wifiPower;

    Log.noticeln("Saved config SSID: '%s'", configuration.wifiSsid);
    Log.noticeln("Saved WiFi Power: %i", configuration.wifiPower);

    EEPROM_saveConfig();

    strcpy(SSID, configuration.wifiSsid);
    WiFi.disconnect(true, true);
    tMillis = millis();
    rebootNeeded = true;
  } else {
    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");
    printHTMLTop(response);
    response->printf_P(htmlWifi);
    printHTMLBottom(response);
    request->send(response);
  }

  intLEDOff();
}

#ifdef TEMP_SENSOR
void CWifiManager::handleSensor(AsyncWebServerRequest *request) {
  Log.traceln("handleSensor: %s", request->methodToString());
  intLEDOn();

  if (request->method() == HTTP_POST) {
    
    uint16_t tempUnit = atoi(request->arg("tempUnit").c_str());
    configuration.tempUnit = tempUnit;
    Log.infoln("Temperature unit: %u", tempUnit);

    tempSensorType tempSensor = (tempSensorType)atoi(request->arg("tempSensor").c_str());
    configuration.tempSensor = tempSensor;
    Log.infoln("Temperature sensor type: %u", tempSensor);

    configuration.tCorrection[0].measured = atoff(request->arg("tMeasured1").c_str());
    configuration.tCorrection[0].actual = atoff(request->arg("tActual1").c_str());
    configuration.tCorrection[1].measured = atoff(request->arg("tMeasured2").c_str());
    configuration.tCorrection[1].actual = atoff(request->arg("tActual2").c_str());

    configuration.hCorrection[0].measured = atoff(request->arg("hMeasured1").c_str());
    configuration.hCorrection[0].actual = atoff(request->arg("hActual1").c_str());
    configuration.hCorrection[1].measured = atoff(request->arg("hMeasured2").c_str());
    configuration.hCorrection[1].actual = atoff(request->arg("hActual2").c_str());

    configuration.voltageDivider = atoff(request->arg("voltageDivider").c_str());

    EEPROM_saveConfig();
    Log.verboseln("Config saved");
    
    request->redirect("sensor");
    tMillis = millis();

    Log.infoln("Requesting reboot...");
    rebootNeeded = true;

  } else {

    char tempUnit[256];
    snprintf(tempUnit, 256, "<option %s value='0'>Celsius</option>\
      <option %s value='1'>Fahrenheit</option>", 
      configuration.tempUnit == TEMP_UNIT_CELSIUS ? "selected" : "", 
      configuration.tempUnit == TEMP_UNIT_FAHRENHEIT ? "selected" : "");
    
    char tempSensor[512] = "";
    snprintf_P(tempSensor, 512, PSTR("\
      <option %s value='0'>-</option>\
      <option %s value='1'>DS18B20</option>\
      <option %s value='2'>BME280</option>\
      <option %s value='3'>DHT22</option>\
      <option %s value='4'>AHT20</option>\
      "), 
      configuration.tempSensor == TEMP_SENSOR_UNSUPPORTED ? "selected" : "", 
      configuration.tempSensor == TEMP_SENSOR_DS18B20 ? "selected" : "", 
      configuration.tempSensor == TEMP_SENSOR_BME280 ? "selected" : "", 
      configuration.tempSensor == TEMP_SENSOR_DHT22 ? "selected" : "", 
      configuration.tempSensor == TEMP_SENSOR_AHT20 ? "selected" : ""
    );

    float t = sensorProvider->getTemperature(NULL);
    if (configuration.tempUnit == TEMP_UNIT_FAHRENHEIT) {
      t = t * 1.8 + 32;
    }

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");
    printHTMLTop(response);
    response->printf_P(htmlSensor, tempSensor, tempUnit,
      t, (configuration.tempUnit == TEMP_UNIT_CELSIUS ? "C" : (configuration.tempUnit == TEMP_UNIT_FAHRENHEIT ? "F" : "" )),
      configuration.tCorrection[0].measured, configuration.tCorrection[0].actual,
      configuration.tCorrection[1].measured, configuration.tCorrection[1].actual,
      sensorProvider->getHumidity(NULL),
      configuration.hCorrection[0].measured, configuration.hCorrection[0].actual,
      configuration.hCorrection[1].measured, configuration.hCorrection[1].actual,
      sensorProvider->getVoltage(NULL), sensorProvider->getVoltageADC(NULL),
      configuration.voltageDivider
    );
    printHTMLBottom(response);
    request->send(response);

  }

  intLEDOff();
}
#endif

void CWifiManager::handleDevice(AsyncWebServerRequest *request) {
  Log.traceln("handleDevice: %s", request->methodToString());
  intLEDOn();

  if (request->method() == HTTP_POST) {
    configuration.ledEnabled = request->hasArg("ledEnabled");

    String deviceName = request->arg("deviceName");
    deviceName.toCharArray(configuration.name, sizeof(configuration.name));
    Log.infoln("Device req name: %s", deviceName);
    Log.infoln("Device size %i name: %s", sizeof(configuration.name), configuration.name);

    EEPROM_saveConfig();
    
    request->redirect("device");
    tMillis = millis();
    rebootNeeded = true;
  } else {

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");
    printHTMLTop(response);
    response->printf_P(htmlDevice, configuration.ledEnabled ? "checked" : "", configuration.name);
    printHTMLBottom(response);
    request->send(response);
  }
  intLEDOff();
}

void CWifiManager::handleFactoryReset(AsyncWebServerRequest *request) {
  Log.traceln("handleFactoryReset");
  intLEDOn();
  
  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8");
  response->setCode(200);
  response->print("OK");

  EEPROM_wipe();
  tMillis = millis();
  rebootNeeded = true;
  
  request->send(response);
  intLEDOff();
}

void CWifiManager::handleReboot(AsyncWebServerRequest *request) {
  Log.traceln("handleReboot");
  intLEDOn();
  
  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8");
  response->setCode(200);
  response->print("OK");

  tMillis = millis();
  rebootNeeded = true;
  
  request->send(response);
  intLEDOff();
}

void CWifiManager::handleRestAPI_LED(AsyncWebServerRequest *request) {
  Log.traceln("handleRestAPI_LED: %s", request->methodToString());
  intLEDOn();
  
  // WORK

  String jsonStr;
  serializeJson(configJson, jsonStr);
  Log.verboseln("API payload: '%s'", jsonStr.c_str());

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8");
  response->print(jsonStr);
  response->setCode(200);
  request->send(response);

  intLEDOff();
}

void CWifiManager::handleRestAPI_Device(AsyncWebServerRequest *request) {
  Log.traceln("handleRestAPI_Device: %s", request->methodToString());
  intLEDOn();
  
  int iv;

  iv = dBmtoPercentage(WiFi.RSSI());
  deviceJson["wifi_percent"] = iv;
  deviceJson["wifi_rssi"] = WiFi.RSSI();

  time_t now; 
  time(&now);
  unsigned long uptimeMillis = CONFIG_getUpTime();

  deviceJson["uptime_millis"] = uptimeMillis;
  // Convert to ISO8601 for JSON
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
  deviceJson["timestamp_iso8601"] = String(buf);
 
  String jsonStr;
  serializeJson(deviceJson, jsonStr);
  Log.verboseln("deviceSettings: '%s'", jsonStr.c_str());

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8");
  response->print(jsonStr);
  response->setCode(200);
  request->send(response);

  intLEDOff();
}

void CWifiManager::handleRestAPI_Config(AsyncWebServerRequest *request) {
  Log.traceln("handleRestAPI_Config: %s", request->methodToString());
  intLEDOn();

  if (request->method() == HTTP_POST) {
    // TODO:
    
    request->redirect("config");
    tMillis = millis();
    rebootNeeded = true;
  } else {
    configJson.clear();

    // Device information
    configJson["name"] = configuration.name;
    configJson["ledEnabled"] = configuration.ledEnabled;

    #ifdef WIFI
    // WiFi settings
    configJson["wifiSsid"] = configuration.wifiSsid;
    configJson["wifiPower"] = configuration.wifiPower;
    
    // NTP settings
    configJson["ntpServer"] = configuration.ntpServer;
    configJson["gmtOffset_sec"] = configuration.gmtOffset_sec;
    configJson["daylightOffset_sec"] = configuration.daylightOffset_sec;
    #endif

    #ifdef LED
    // LED settings
    configJson["ledBrightness"] = configuration.ledBrightness;
    configJson["ledMode"] = configuration.ledMode;
    configJson["ledDelayMs"] = configuration.ledDelayMs;
    configJson["ledCycleModeMs"] = configuration.ledCycleModeMs;
    configJson["ledStripSize"] = configuration.ledStripSize;
    configJson["psLedBrightness"] = configuration.psLedBrightness;
    configJson["psStartHour"] = configuration.psStartHour;
    configJson["psEndHour"] = configuration.psEndHour;
    #endif

    String jsonStr;
    serializeJson(configJson, jsonStr);
    Log.verboseln("Config: '%s'", jsonStr.c_str());

    AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8");
    response->print(jsonStr);
    response->setCode(200);
    request->send(response);
  }
  intLEDOff();
}

void CWifiManager::handleStyleCSS(AsyncWebServerRequest *request) {
  Log.traceln("handleStyleCSS");
  static uint32_t dataLen = strlen_P(cssPico);
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/css; charset=UTF-8", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t len = (dataLen>maxLen)?maxLen:dataLen;
    if (len > 0) {
      memcpy_P(buffer, cssPico + index, len);
      dataLen -= len;
    } else {
      dataLen = strlen_P(cssPico);
    }
    return len;
  });
  request->send(response);
}

bool CWifiManager::isApMode() { 

#if defined(ESP32)
  return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_MODE_APSTA; 
#elif defined(ESP8266)
  return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA; 
#endif

}

void CWifiManager::printHTMLTop(Print *p) {
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  p->printf_P(htmlTop, 
    configuration.name, 
    isApMode() ? softAP_SSID : SSID, dBmtoPercentage(WiFi.RSSI()),
    hr, min % 60, sec % 60,
    configuration.name
  );
}

void CWifiManager::printHTMLBottom(Print *p) {
  String jsonStr;
  serializeJson(deviceJson, jsonStr);
  p->printf_P(htmlBottom, VERSION, jsonStr.c_str());
}

void CWifiManager::printHTMLMain(Print *p) {

  String modeOptions = "";
  if (modes != NULL) {
    for(uint8_t i=0; i<modes->size(); i++) {
      modeOptions += String("<option") + String(i == configuration.ledMode ? " selected" : "") + String(" value='") + String(i) + String("'>") + (*modes)[i]->getName() + String("</option>");
    }
  }

  p->printf_P(htmlMain, 
    configuration.ledStripSize,
    modeOptions.c_str(), 
    configuration.ledBrightness, 
    configuration.ledDelayMs, 
    configuration.ledCycleModeMs / 1000
  );

}

bool CWifiManager::updateConfigFromJson(JsonDocument jsonObj) {

  if (!jsonObj["name"].isNull()) {
    Log.traceln("Setting 'name' to %s", jsonObj["name"].as<const char*>());
    strncpy(configuration.name, jsonObj["name"].as<const char*>(), 128);
  }

  // TODO: other settings

  return true;
}
