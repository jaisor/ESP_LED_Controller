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
#include "Device.h"

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
  server->on("/", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleRoot(request); });
  server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStyleCSS(request); });
  server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(404); });
  //

  server->on("/wifi", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleWifi(request); });
  server->on("/device", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleDevice(request); });
  //
  server->on("/factory_reset", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFactoryReset(request); });
  server->on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot(request); });
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
  server->on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRestAPI_Config(request);
  });
  AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    bool success = this->updateConfigFromJson(json.as<JsonObject>());
    if (success) {
      EEPROM_saveConfig();
      
      // Return updated configuration
      handleRestAPI_Config(request);
      
      // Schedule reboot if needed for certain changes
      tMillis = millis();
      rebootNeeded = true;
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8");
      response->print("ERROR: Invalid configuration data");
      response->setCode(400);
      request->send(response);
    }
  });
  server->addHandler(configHandler);
  
  server->on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRestAPI_LED(request);
  });
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
  
  #ifdef OLED
  if (device) {
    if (isApMode()) {
      //device->displayApInfo(softAP_SSID, WIFI_FALLBACK_PASS, WiFi.softAPIP().toString().c_str());
    } else {
      //device->displayWifiInfo(SSID, WiFi.localIP().toString().c_str());
    }
  }
  #endif
  
  // NTP
  Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);
  configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Log.noticeln("The time is %i:%i", timeinfo.tm_hour,timeinfo.tm_min);
  }
  CONFIG_getLedBrightness(true);

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

    #ifdef OLEDA
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

  if (request->method() == HTTP_POST) {

    // LED Settings
    uint8_t ledType = atoi(request->arg("ledType").c_str());
    if (ledType < 13) { // We have 13 LED types
      if (configuration.ledType != ledType) {
        Log.noticeln("ledType: '%i'", ledType);
        configuration.ledType = ledType;
        tMillis = millis();
        rebootNeeded = true;
      }
    }
    
    if (modes != NULL) {
      uint8_t ledMode = atoi(request->arg("ledMode").c_str());
      if (ledMode<modes->size()) {
        configuration.ledMode = ledMode;
      }
    }

    float ledBrightness = atof(request->arg("ledBrightness").c_str())/100.0;
    if (ledBrightness>=0.0 && ledBrightness<=1.0) {
      configuration.ledBrightness = ledBrightness;
    }

    configuration.ledDelayMs = atol(request->arg("ledDelayMs").c_str());
    configuration.ledCycleModeMs = atol(request->arg("ledCycleModeMs").c_str()) * 1000;
      
    Log.noticeln("ledMode: '%i'", configuration.ledMode);
    Log.noticeln("ledBrightness: '%D'", configuration.ledBrightness);
    Log.noticeln("ledDelayMs: '%lu'", configuration.ledDelayMs);
    Log.noticeln("ledCycleModeMs: '%lu'", configuration.ledCycleModeMs);

    uint16_t ledStripSize = atol(request->arg("ledStripSize").c_str());
    if (configuration.ledStripSize != ledStripSize) {
      Log.noticeln("ledStripSize: '%i'", ledStripSize);
      configuration.ledStripSize = ledStripSize;
      tMillis = millis();
      rebootNeeded = true;
    }

    // Power-save settings
    float psLedBrightness = atof(request->arg("psLedBrightness").c_str())/100.0;
    if (psLedBrightness>=0.0 && psLedBrightness<=1.0) {
      configuration.psLedBrightness = psLedBrightness;
      Log.noticeln("psLedBrightness: %D", configuration.psLedBrightness);
    }

    int psStartHour = atoi(request->arg("psStartHour").c_str());
    if (psStartHour >= 0 && psStartHour < 23) {  
      configuration.psStartHour = psStartHour;
      Log.noticeln("psStartHour: %i", psStartHour);
    }

    int psEndHour = atoi(request->arg("psEndHour").c_str());
    if (psEndHour >= 0 && psEndHour < 23) {  
      configuration.psEndHour = psEndHour;
      Log.noticeln("psEndHour: %i", psEndHour);
    }

    // Mode cycling configuration
    bool cycleAllModes = request->hasArg("cycleAllModes");
    if (cycleAllModes) {
      configuration.cycleModesCount = 0;
      Log.noticeln("Cycle all modes enabled");
    } else if (request->hasArg("cycleModesList")) {
      String modesList = request->arg("cycleModesList");
      modesList.trim();
      if (modesList.length() > 0 && modes != NULL) {
        uint8_t count = 0;
        int startPos = 0;
        int commaPos = modesList.indexOf(',');
        
        while (startPos < modesList.length() && count < 32) {
          String modeStr;
          if (commaPos == -1) {
            modeStr = modesList.substring(startPos);
          } else {
            modeStr = modesList.substring(startPos, commaPos);
          }
          modeStr.trim();
          
          if (modeStr.length() > 0) {
            uint8_t modeIndex = atoi(modeStr.c_str());
            if (modeIndex < modes->size()) {
              configuration.cycleModesList[count++] = modeIndex;
              Log.verboseln("Added mode %d to cycle list", modeIndex);
            }
          }
          
          if (commaPos == -1) break;
          startPos = commaPos + 1;
          commaPos = modesList.indexOf(',', startPos);
        }
        
        configuration.cycleModesCount = count;
        Log.noticeln("Custom cycle with %d modes", count);
      }
    }

    //

    Log.noticeln("Saving configuration...");
    EEPROM_saveConfig();

    Log.noticeln("Forcing LED brightness update...");
    CONFIG_getLedBrightness(true);
    
    Log.noticeln("Redirecting to root");
    request->redirect("/");
  
  } else {
    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8");
    printHTMLTop(response);
    printHTMLMain(response);
    printHTMLBottom(response);
    request->send(response);
  }

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
  configJson["ledType"] = configuration.ledType;
  configJson["ledDelayMs"] = configuration.ledDelayMs;
  configJson["ledCycleModeMs"] = configuration.ledCycleModeMs;
  configJson["ledStripSize"] = configuration.ledStripSize;
  configJson["psLedBrightness"] = configuration.psLedBrightness;
  configJson["psStartHour"] = configuration.psStartHour;
  configJson["psEndHour"] = configuration.psEndHour;
  configJson["cycleModesCount"] = configuration.cycleModesCount;
  JsonArray cycleModes = configJson["cycleModesList"].to<JsonArray>();
  for (uint8_t i = 0; i < configuration.cycleModesCount; i++) {
    cycleModes.add(configuration.cycleModesList[i]);
  }
  #endif

  String jsonStr;
  serializeJson(configJson, jsonStr);
  Log.verboseln("Config: '%s'", jsonStr.c_str());

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8");
  response->print(jsonStr);
  response->setCode(200);
  request->send(response);

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

  String typeOptions = "";
  const char* ledTypes[] = {"WS2812B", "WS2812", "WS2813", "WS2815", "SK6812", "TM1809", "TM1804", "TM1803", "UCS1903", "UCS1904", "GS1903", "PL9823", "WS2852", "WS2811"};
  const uint8_t numLedTypes = sizeof(ledTypes) / sizeof(ledTypes[0]);
  for (uint8_t i = 0; i < numLedTypes; i++) {
    typeOptions += String("<option") + String(i == configuration.ledType ? " selected" : "") + String(" value='") + String(i) + String("'>") + String(ledTypes[i]) + String("</option>");
  }

  String modeOptions = "";
  String currentModeName = "Unknown";
  if (modes != NULL) {
    for(uint8_t i=0; i<modes->size(); i++) {
      modeOptions += String("<option") + String(i == configuration.ledMode ? " selected" : "") + String(" value='") + String(i) + String("'>") + (*modes)[i]->getName() + String("</option>");
    }
    if (configuration.ledMode < modes->size()) {
      currentModeName = (*modes)[configuration.ledMode]->getName();
    }
  }

  // Calculate remaining time
  String timeRemaining;
  if (configuration.ledCycleModeMs > 0) {
    unsigned long elapsed = millis() - lastModeChangeMs;
    if (elapsed < configuration.ledCycleModeMs) {
      unsigned long remaining = (configuration.ledCycleModeMs - elapsed) / 1000;
      timeRemaining = String(remaining) + String(" seconds");
    } else {
      timeRemaining = "changing now...";
    }
  } else {
    timeRemaining = "indefinite";
  }

  // Build cycle modes list string
  String cycleModesListStr = "";
  if (configuration.cycleModesCount > 0) {
    for (uint8_t i = 0; i < configuration.cycleModesCount; i++) {
      if (i > 0) cycleModesListStr += ",";
      cycleModesListStr += String(configuration.cycleModesList[i]);
    }
  }

  // Build available modes list string
  String availableModesStr = "";
  if (modes != NULL) {
    for(uint8_t i=0; i<modes->size(); i++) {
      if (i > 0) availableModesStr += ", ";
      availableModesStr += String(i) + "=" + (*modes)[i]->getName();
    }
  }

  p->printf_P(htmlMain, 
    currentModeName.c_str(),
    timeRemaining.c_str(),
    configuration.ledStripSize,
    typeOptions.c_str(),
    modeOptions.c_str(), 
    configuration.ledBrightness * 100, configuration.ledBrightness * 100, 
    configuration.ledDelayMs, 
    configuration.ledCycleModeMs / 1000,
    configuration.cycleModesCount == 0 ? "checked" : "",
    cycleModesListStr.c_str(),
    configuration.cycleModesCount == 0 ? "disabled" : "",
    availableModesStr.c_str(),
    configuration.psLedBrightness * 100, configuration.psLedBrightness * 100, 
    configuration.psStartHour, 
    configuration.psEndHour
  );

}

bool CWifiManager::updateConfigFromJson(JsonDocument jsonObj) {

  // Device information
  if (!jsonObj["name"].isNull()) {
    Log.traceln("Setting 'name' to %s", jsonObj["name"].as<const char*>());
    strncpy(configuration.name, jsonObj["name"].as<const char*>(), sizeof(configuration.name) - 1);
    configuration.name[sizeof(configuration.name) - 1] = '\0';
  }

  if (!jsonObj["ledEnabled"].isNull()) {
    configuration.ledEnabled = jsonObj["ledEnabled"].as<bool>();
    Log.traceln("Setting 'ledEnabled' to %d", configuration.ledEnabled);
  }

  #ifdef WIFI
  // WiFi settings
  if (!jsonObj["wifiSsid"].isNull()) {
    strncpy(configuration.wifiSsid, jsonObj["wifiSsid"].as<const char*>(), sizeof(configuration.wifiSsid) - 1);
    configuration.wifiSsid[sizeof(configuration.wifiSsid) - 1] = '\0';
    Log.traceln("Setting 'wifiSsid' to %s", configuration.wifiSsid);
    rebootNeeded = true;
  }

  if (!jsonObj["wifiPassword"].isNull()) {
    strncpy(configuration.wifiPassword, jsonObj["wifiPassword"].as<const char*>(), sizeof(configuration.wifiPassword) - 1);
    configuration.wifiPassword[sizeof(configuration.wifiPassword) - 1] = '\0';
    Log.traceln("Setting 'wifiPassword'");
    rebootNeeded = true;
  }

  if (!jsonObj["wifiPower"].isNull()) {
    configuration.wifiPower = jsonObj["wifiPower"].as<int8_t>();
    Log.traceln("Setting 'wifiPower' to %d", configuration.wifiPower);
    rebootNeeded = true;
  }

  // NTP settings
  if (!jsonObj["ntpServer"].isNull()) {
    strncpy(configuration.ntpServer, jsonObj["ntpServer"].as<const char*>(), sizeof(configuration.ntpServer) - 1);
    configuration.ntpServer[sizeof(configuration.ntpServer) - 1] = '\0';
    Log.traceln("Setting 'ntpServer' to %s", configuration.ntpServer);
    rebootNeeded = true;
  }

  if (!jsonObj["gmtOffset_sec"].isNull()) {
    configuration.gmtOffset_sec = jsonObj["gmtOffset_sec"].as<long>();
    Log.traceln("Setting 'gmtOffset_sec' to %l", configuration.gmtOffset_sec);
    rebootNeeded = true;
  }

  if (!jsonObj["daylightOffset_sec"].isNull()) {
    configuration.daylightOffset_sec = jsonObj["daylightOffset_sec"].as<int>();
    Log.traceln("Setting 'daylightOffset_sec' to %d", configuration.daylightOffset_sec);
    rebootNeeded = true;
  }
  #endif

  #ifdef LED
  // LED settings
  if (!jsonObj["ledBrightness"].isNull()) {
    float brightness = jsonObj["ledBrightness"].as<float>();
    if (brightness >= 0.0 && brightness <= 1.0) {
      configuration.ledBrightness = brightness;
      Log.traceln("Setting 'ledBrightness' to %D", configuration.ledBrightness);
    }
  }

  if (!jsonObj["ledMode"].isNull()) {
    uint8_t mode = jsonObj["ledMode"].as<uint8_t>();
    if (modes != NULL && mode < modes->size()) {
      configuration.ledMode = mode;
      Log.traceln("Setting 'ledMode' to %d", configuration.ledMode);
    }
  }

  if (!jsonObj["ledType"].isNull()) {
    uint8_t type = jsonObj["ledType"].as<uint8_t>();
    if (type < 13) { // We have 13 LED types
      if (configuration.ledType != type) {
        configuration.ledType = type;
        Log.traceln("Setting 'ledType' to %d", configuration.ledType);
        rebootNeeded = true;
      }
    }
  }

  if (!jsonObj["ledDelayMs"].isNull()) {
    configuration.ledDelayMs = jsonObj["ledDelayMs"].as<unsigned long>();
    Log.traceln("Setting 'ledDelayMs' to %l", configuration.ledDelayMs);
  }

  if (!jsonObj["ledCycleModeMs"].isNull()) {
    configuration.ledCycleModeMs = jsonObj["ledCycleModeMs"].as<unsigned long>();
    Log.traceln("Setting 'ledCycleModeMs' to %l", configuration.ledCycleModeMs);
  }

  if (!jsonObj["ledStripSize"].isNull()) {
    configuration.ledStripSize = jsonObj["ledStripSize"].as<uint16_t>();
    Log.traceln("Setting 'ledStripSize' to %d", configuration.ledStripSize);
  }

  if (!jsonObj["psLedBrightness"].isNull()) {
    float psBrightness = jsonObj["psLedBrightness"].as<float>();
    if (psBrightness >= 0.0 && psBrightness <= 1.0) {
      configuration.psLedBrightness = psBrightness;
      Log.traceln("Setting 'psLedBrightness' to %D", configuration.psLedBrightness);
    }
  }

  if (!jsonObj["psStartHour"].isNull()) {
    int8_t hour = jsonObj["psStartHour"].as<int8_t>();
    if (hour >= 0 && hour <= 23) {
      configuration.psStartHour = hour;
      Log.traceln("Setting 'psStartHour' to %d", configuration.psStartHour);
    }
  }

  if (!jsonObj["psEndHour"].isNull()) {
    int8_t hour = jsonObj["psEndHour"].as<int8_t>();
    if (hour >= 0 && hour <= 23) {
      configuration.psEndHour = hour;
      Log.traceln("Setting 'psEndHour' to %d", configuration.psEndHour);
    }
  }

  // Mode cycling configuration
  if (!jsonObj["cycleModesList"].isNull() && jsonObj["cycleModesList"].is<JsonArray>()) {
    JsonArray modeList = jsonObj["cycleModesList"].as<JsonArray>();
    uint8_t count = min((size_t)32, modeList.size());
    configuration.cycleModesCount = count;
    
    for (uint8_t i = 0; i < count; i++) {
      uint8_t modeIndex = modeList[i].as<uint8_t>();
      if (modes != NULL && modeIndex < modes->size()) {
        configuration.cycleModesList[i] = modeIndex;
      } else {
        configuration.cycleModesList[i] = 0;
      }
    }
    Log.traceln("Setting 'cycleModesList' with %d modes", configuration.cycleModesCount);
  }

  if (!jsonObj["cycleModesCount"].isNull()) {
    uint8_t count = jsonObj["cycleModesCount"].as<uint8_t>();
    if (count <= 32) {
      configuration.cycleModesCount = count;
      Log.traceln("Setting 'cycleModesCount' to %d", configuration.cycleModesCount);
    }
  }

  // Update LED brightness calculation if power-save settings changed
  CONFIG_getLedBrightness(true);
  #endif

  return true;
}
