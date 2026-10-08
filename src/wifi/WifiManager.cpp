#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 or ESP32 platform!
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <time.h>
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

// Timezone table: { gmtOffset_sec, POSIX TZ string, human-readable label }
struct TimezoneEntry {
  long offset;
  const char* posix;
  const char* label;
};

static const TimezoneEntry timezones[] PROGMEM = {
  { -43200, "UTC12",                              "(UTC-12:00) Baker Island" },
  { -39600, "SST11",                               "(UTC-11:00) American Samoa" },
  { -36000, "HST10",                                "(UTC-10:00) Hawaii" },
  { -32400, "AKST9AKDT,M3.2.0,M11.1.0",            "(UTC-09:00) Alaska" },
  { -28800, "PST8PDT,M3.2.0,M11.1.0",              "(UTC-08:00) Pacific Time (US & Canada)" },
  { -25200, "MST7MDT,M3.2.0,M11.1.0",              "(UTC-07:00) Mountain Time (US & Canada)" },
  { -21600, "CST6CDT,M3.2.0,M11.1.0",              "(UTC-06:00) Central Time (US & Canada)" },
  { -18000, "EST5EDT,M3.2.0,M11.1.0",              "(UTC-05:00) Eastern Time (US & Canada)" },
  { -14400, "AST4ADT,M3.2.0,M11.1.0",              "(UTC-04:00) Atlantic Time (Canada)" },
  { -12600, "NST3:30NDT,M3.2.0,M11.1.0",           "(UTC-03:30) Newfoundland" },
  { -10800, "BRT3",                                 "(UTC-03:00) Buenos Aires, Brasilia" },
  {  -7200, "FNT2",                                 "(UTC-02:00) Mid-Atlantic" },
  {  -3600, "AZOT1AZOST,M3.5.0/0,M10.5.0/1",       "(UTC-01:00) Azores" },
  {      0, "GMT0",                                 "(UTC+00:00) GMT / London, Dublin" },
  {   3600, "CET-1CEST,M3.5.0,M10.5.0/3",          "(UTC+01:00) Central European Time" },
  {   7200, "EET-2EEST,M3.5.0/3,M10.5.0/4",        "(UTC+02:00) Eastern European Time" },
  {  10800, "MSK-3",                                "(UTC+03:00) Moscow, Istanbul" },
  {  12600, "IRST-3:30IRDT,J79/24,J263/24",         "(UTC+03:30) Tehran" },
  {  14400, "GST-4",                                "(UTC+04:00) Dubai, Baku" },
  {  16200, "AFT-4:30",                             "(UTC+04:30) Kabul" },
  {  18000, "PKT-5",                                "(UTC+05:00) Karachi, Tashkent" },
  {  19800, "IST-5:30",                             "(UTC+05:30) Mumbai, New Delhi" },
  {  20700, "NPT-5:45",                             "(UTC+05:45) Kathmandu" },
  {  21600, "BST-6",                                "(UTC+06:00) Dhaka, Almaty" },
  {  23400, "MMT-6:30",                             "(UTC+06:30) Yangon" },
  {  25200, "ICT-7",                                "(UTC+07:00) Bangkok, Jakarta" },
  {  28800, "CST-8",                                "(UTC+08:00) Beijing, Singapore, Perth" },
  {  32400, "JST-9",                                "(UTC+09:00) Tokyo, Seoul" },
  {  34200, "ACST-9:30ACDT,M10.1.0,M4.1.0/3",      "(UTC+09:30) Adelaide, Darwin" },
  {  36000, "AEST-10AEDT,M10.1.0,M4.1.0/3",        "(UTC+10:00) Sydney, Melbourne" },
  {  39600, "SBT-11",                               "(UTC+11:00) Solomon Islands" },
  {  43200, "NZST-12NZDT,M9.5.0,M4.1.0/3",         "(UTC+12:00) Auckland, Fiji" },
  {  46800, "TOT-13",                               "(UTC+13:00) Tonga, Samoa" },
};

static const int TIMEZONE_COUNT = sizeof(timezones) / sizeof(timezones[0]);

/**
 * Get POSIX timezone string for automatic DST handling
 * Looks up offset in timezone table, falls back to UTC+/-N
 */
const char* getTzString(long gmtOffset_sec) {
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    if (timezones[i].offset == gmtOffset_sec) {
      return timezones[i].posix;
    }
  }
  // Fallback for offsets not in the table
  static char tzStr[16];
  int hours = gmtOffset_sec / 3600;
  snprintf(tzStr, sizeof(tzStr), "UTC%+d", -hours);
  return tzStr;
}

CWifiManager::CWifiManager()
:rebootNeeded(false), wifiRetries(0), modes(nullptr), ledLayout(LED_LAYOUT_SINGLE), device(nullptr) {

  deviceJson["dev_name"] = configuration.name;
  deviceJson["version"] = VERSION;
  deviceJson["version_short"] = VERSION_SHORT;
  deviceJson["chip_model"] = CONFIG_getChipModel();
  deviceJson["chip_revision"] = CONFIG_getChipRevision();
  deviceJson["flash_size_bytes"] = CONFIG_getFlashChipSize();

  strcpy(SSID, configuration.wifiSsid);
  server = new AsyncWebServer(WEB_SERVER_PORT);
  connect();
}

// WiFi.localIP() is 0.0.0.0 while running as a soft AP, so the reachable address has to be
// picked per mode rather than read from localIP() unconditionally.
String CWifiManager::currentIP() {
  return isApMode() ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
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
    WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable modem sleep to prevent beacon-interval stalls in main loop
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
  server->on("/led", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleLED(request); });
  //
  server->on("/factory_reset", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFactoryReset(request); });
  server->on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot(request); });
#ifdef WEB_LOGGING
  server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log.traceln("handleLog");
    intLEDOn();
    AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 16384);
    response->println(logStream.str().c_str());
    logStream.str(""); // Clear after reading to free memory
    request->send(response);
    intLEDOff();
  });
#endif
  server->on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRestAPI_Config(request);
  });
  AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    String error;
    bool success = this->updateConfigFromJson(json.as<JsonObject>(), true, &error);
    if (success) {
      EEPROM_saveConfig();
      
      // Return updated configuration
      handleRestAPI_Config(request);
      
      // Schedule reboot if needed for certain changes
      tMillis = millis();
      rebootNeeded = true;
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 128);
      response->printf("ERROR: Invalid configuration data: %s", error.c_str());
      response->setCode(400);
      request->send(response);
    }
  });
  server->addHandler(configHandler);
  
  server->on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleRestAPI_LED(request);
  });
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/api", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    // Live changes only - LED hardware fields need a reboot, so they go through /config or /led
    bool success = this->updateConfigFromJson(json.as<JsonObject>(), false);
    if (success) {
      handleRestAPI_LED(request);
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
      response->print("ERROR");
      response->setCode(500);
      request->send(response);
    }
  });
  server->addHandler(handler);

  server->begin();
  Log.infoln("Web server listening on http://%s:%i", currentIP().c_str(), WEB_SERVER_PORT);

  deviceJson["ip"] = currentIP();
  deviceJson["mac_address"] = WiFi.macAddress();
  
  #ifdef OLED
  if (device) {
    if (isApMode()) {
      device->setWifiAPInfo(softAP_SSID, WIFI_FALLBACK_PASS, WiFi.softAPIP().toString().c_str());
    } else {
      device->setWifiConnectedInfo(WiFi.localIP().toString().c_str());
    }
  }
  #endif
  
  // NTP - Using POSIX timezone string for automatic DST handling
  const char* timezone = getTzString(configuration.gmtOffset_sec);
  Log.infoln("Configuring time from %s with timezone %s (GMT offset: %i)", configuration.ntpServer, timezone, configuration.gmtOffset_sec);
  configTzTime(timezone, configuration.ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo, 100)){ // Short timeout - NTP may not be available in AP mode
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
      if (isApMode()) {
        Log.noticeln("WiFi AP '%s' ready, IP address: %s", softAP_SSID, WiFi.softAPIP().toString().c_str());
      } else {
        Log.noticeln("WiFi connected to '%s', IP address: %s, gateway: %s, subnet: %s, RSSI: %i dBm",
          WiFi.SSID().c_str(),
          WiFi.localIP().toString().c_str(),
          WiFi.gatewayIP().toString().c_str(),
          WiFi.subnetMask().toString().c_str(),
          WiFi.RSSI());
      }
      // Start listening for requests
      if (device) {
        device->setState(isApMode() ? DeviceState::WIFI_AP_CREATED : DeviceState::WIFI_CONNECTED);
      }
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
    if (device) {
      device->setState(DeviceState::WIFI_OFFLINE);
    }
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

    // LED Settings (strip hardware is set at /led)
    uint8_t ledMode = atoi(request->arg("ledMode").c_str());
    if (isModeSelectable(ledMode, ledLayout)) {
      configuration.ledMode = ledMode;
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
            if (isModeSelectable(modeIndex, ledLayout)) {
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
    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 4096);
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

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 2048);

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
    // Generate WiFi power options with current selection marked
    char wifiPowerOptions[1024];
    snprintf_P(wifiPowerOptions, sizeof(wifiPowerOptions), PSTR(
      "<option %s value='78'>WIFI_POWER_19_5dBm (7.8dBm)</option>"
      "<option %s value='76'>WIFI_POWER_19dBm (7.6dBm)</option>"
      "<option %s value='74'>WIFI_POWER_18_5dBm (7.4dBm)</option>"
      "<option %s value='68'>WIFI_POWER_17dBm (6.8dBm)</option>"
      "<option %s value='60'>WIFI_POWER_15dBm (6.0dBm)</option>"
      "<option %s value='52'>WIFI_POWER_13dBm (5.2dBm)</option>"
      "<option %s value='44'>WIFI_POWER_11dBm (4.4dBm)</option>"
      "<option %s value='34'>WIFI_POWER_8_5dBm (3.4dBm)</option>"
      "<option %s value='28'>WIFI_POWER_7dBm (2.8dBm)</option>"
      "<option %s value='20'>WIFI_POWER_5dBm (2.0dBm)</option>"
      "<option %s value='8'>WIFI_POWER_2dBm (0.8dBm)</option>"
      "<option %s value='-4'>WIFI_POWER_MINUS_1dBm (-0.4dBm)</option>"),
      configuration.wifiPower == 78  ? "selected" : "",
      configuration.wifiPower == 76  ? "selected" : "",
      configuration.wifiPower == 74  ? "selected" : "",
      configuration.wifiPower == 68  ? "selected" : "",
      configuration.wifiPower == 60  ? "selected" : "",
      configuration.wifiPower == 52  ? "selected" : "",
      configuration.wifiPower == 44  ? "selected" : "",
      configuration.wifiPower == 34  ? "selected" : "",
      configuration.wifiPower == 28  ? "selected" : "",
      configuration.wifiPower == 20  ? "selected" : "",
      configuration.wifiPower == 8   ? "selected" : "",
      configuration.wifiPower == -4  ? "selected" : ""
    );
    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 6144);
    printHTMLTop(response);
    response->printf_P(htmlWifi, configuration.wifiSsid, wifiPowerOptions);
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

    if (request->hasArg("timezone")) {
      long newOffset = atol(request->arg("timezone").c_str());
      if (newOffset != configuration.gmtOffset_sec) {
        configuration.gmtOffset_sec = newOffset;
        Log.infoln("Timezone offset set to: %l", configuration.gmtOffset_sec);
        // Reconfigure NTP with new timezone
        const char* tz = getTzString(configuration.gmtOffset_sec);
        configTzTime(tz, configuration.ntpServer);
      }
    }

    EEPROM_saveConfig();
    
    request->redirect("device");
    tMillis = millis();
    rebootNeeded = true;
  } else {

    // Build timezone dropdown options
    String tzOptions;
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
      tzOptions += "<option value='";
      tzOptions += timezones[i].offset;
      tzOptions += "'";
      if (timezones[i].offset == configuration.gmtOffset_sec) {
        tzOptions += " selected";
      }
      tzOptions += ">";
      tzOptions += timezones[i].label;
      tzOptions += "</option>";
    }

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 6144);
    printHTMLTop(response);
    response->printf_P(htmlDevice,
      CONFIG_getChipModel().c_str(), CONFIG_getChipRevision(),
      CONFIG_getFlashChipSize() / (1024 * 1024), WiFi.macAddress().c_str(),
      configuration.ledEnabled ? "checked" : "", configuration.name, tzOptions.c_str(), configuration.name);
    printHTMLBottom(response);
    request->send(response);
  }
  intLEDOff();
}

static void appendOption(String &out, const String &value, const String &label, bool selected) {
  out += "<option value='";
  out += value;
  out += selected ? "' selected>" : "'>";
  out += label;
  out += "</option>";
}

static String pinOptions(uint8_t selectedPin) {
  String options;
  for (uint8_t i = 0; i < LED_PIN_COUNT; i++) {
    if (CONFIG_isValidLedPin(LED_PINS[i])) {
      appendOption(options, String(LED_PINS[i]), String("GPIO ") + LED_PINS[i], LED_PINS[i] == selectedPin);
    }
  }
  return options;
}

// One line description of the configured LED hardware, e.g. "Ring light: 267 LEDs (141 outer, 126 inner) on GPIO 12"
static String ledHardwareSummary() {
  String s = String(LED_LAYOUT_LABELS[configuration.ledLayout]) + ": ";
  switch (configuration.ledLayout) {
    case LED_LAYOUT_DUAL:
      if (configuration.ledMirror) {
        s += String(configuration.ledStripSize) + " LEDs on GPIO " + configuration.ledPin + ", mirrored on GPIO " + configuration.ledPin2;
      } else {
        s += String(configuration.ledStripSize) + " + " + configuration.ledStripSize2 + " LEDs on GPIO " + configuration.ledPin + " and GPIO " + configuration.ledPin2;
      }
      break;
    case LED_LAYOUT_RING:
      s += String(configuration.ledStripSize) + " LEDs (" + configuration.ledRingOuterSize + " outer, "
        + (configuration.ledStripSize - configuration.ledRingOuterSize) + " inner) on GPIO " + configuration.ledPin;
      break;
    default:
      s += String(configuration.ledStripSize) + " LEDs on GPIO " + configuration.ledPin;
  }
  s += String(", ") + LED_TYPE_NAMES[configuration.ledType] + " " + CONFIG_ledColorOrderName(configuration.ledColorOrder);
  return s;
}

void CWifiManager::handleLED(AsyncWebServerRequest *request) {
  Log.traceln("handleLED: %s", request->methodToString());
  intLEDOn();

  if (request->method() == HTTP_POST) {
    // Run the form through the same validation as an imported configuration
    JsonDocument json;
    const char *stringFields[] = {"ledLayout", "ledColorOrder"};
    for (const char *field : stringFields) {
      if (request->hasArg(field)) {
        json[field] = request->arg(field);
      }
    }
    const char *numberFields[] = {"ledType", "ledPin", "ledPin2", "ledStripSize", "ledStripSize2", "ledRingOuterSize"};
    for (const char *field : numberFields) {
      if (request->hasArg(field)) {
        json[field] = request->arg(field).toInt();
      }
    }
    if (request->arg("ledLayout") == LED_LAYOUT_IDS[LED_LAYOUT_DUAL]) {
      json["ledMirror"] = request->hasArg("ledMirror");
    }

    String error;
    if (updateConfigFromJson(json, true, &error)) {
      EEPROM_saveConfig();
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
      response->print("OK");
      request->send(response);
      tMillis = millis();
      rebootNeeded = true;
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 128);
      response->setCode(400);
      response->print(error);
      request->send(response);
    }
  } else {
    String layoutOptions;
    for (uint8_t i = 0; i < LED_LAYOUT_COUNT; i++) {
      appendOption(layoutOptions, LED_LAYOUT_IDS[i], LED_LAYOUT_LABELS[i], i == configuration.ledLayout);
    }
    String typeOptions;
    for (uint8_t i = 0; i < LED_TYPE_COUNT; i++) {
      appendOption(typeOptions, String(i), LED_TYPE_NAMES[i], i == configuration.ledType);
    }
    String orderOptions;
    for (uint8_t i = 0; i < LED_COLOR_ORDER_COUNT; i++) {
      appendOption(orderOptions, LED_COLOR_ORDERS[i].name, LED_COLOR_ORDERS[i].name, LED_COLOR_ORDERS[i].value == configuration.ledColorOrder);
    }

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 8192);
    printHTMLTop(response);
    response->printf_P(htmlLed,
      ledHardwareSummary().c_str(),
      layoutOptions.c_str(), typeOptions.c_str(), orderOptions.c_str(),
      pinOptions(configuration.ledPin).c_str(), LED_MAX_COUNT, configuration.ledStripSize,
      configuration.ledRingOuterSize,
      configuration.ledMirror ? "checked" : "",
      pinOptions(configuration.ledPin2).c_str(), LED_MAX_COUNT, configuration.ledStripSize2);
    printHTMLBottom(response);
    request->send(response);
  }

  intLEDOff();
}

bool CWifiManager::isModeSelectable(uint8_t index, uint8_t layout) {
  return modes != NULL && index < modes->size() && (*modes)[index]->supportsLayout(layout);
}

void CWifiManager::handleFactoryReset(AsyncWebServerRequest *request) {
  Log.traceln("handleFactoryReset");
  intLEDOn();
  
  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
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
  
  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
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

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 2048);
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

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 2048);
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
  configJson["ledLayout"] = LED_LAYOUT_IDS[configuration.ledLayout];
  configJson["ledPin"] = configuration.ledPin;
  configJson["ledPin2"] = configuration.ledPin2;
  configJson["ledColorOrder"] = CONFIG_ledColorOrderName(configuration.ledColorOrder);
  configJson["ledStripSize2"] = configuration.ledStripSize2;
  configJson["ledRingOuterSize"] = configuration.ledRingOuterSize;
  configJson["ledMirror"] = (bool)configuration.ledMirror;
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

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 2048);
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

  // Only modes that fit the running layout are offered
  String modeOptions = "";
  String currentModeName = "Unknown";
  if (modes != NULL) {
    for(uint8_t i=0; i<modes->size(); i++) {
      if (!isModeSelectable(i, ledLayout)) continue;
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
      if (!isModeSelectable(i, ledLayout)) continue;
      if (availableModesStr.length() > 0) availableModesStr += ", ";
      availableModesStr += String(i) + "=" + (*modes)[i]->getName();
    }
  }

  // Get current local time if WiFi connected
  String currentTimeStr = "";
  if (device && device->getState() == DeviceState::WIFI_CONNECTED) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
      char timeBuf[64];
      strftime(timeBuf, sizeof(timeBuf), "%A, %B %d %Y %I:%M %p", &timeinfo);
      currentTimeStr = String("<p>\xF0\x9F\x95\x92 Current local time: <b>") + String(timeBuf) + String("</b></p>");
    }
  }
  
  p->printf_P(htmlMain, 
    currentModeName.c_str(),
    timeRemaining.c_str(),
    currentTimeStr.c_str(),
    ledHardwareSummary().c_str(),
    modeOptions.c_str(), 
    configuration.ledBrightness * 100, configuration.ledBrightness * 100, 
    configuration.ledDelayMs, 
    configuration.ledCycleModeMs / 1000,
    configuration.cycleModesCount == 0 ? "checked" : "",
    cycleModesListStr.c_str(),
    configuration.cycleModesCount == 0 ? "disabled" : "",
    availableModesStr.c_str(),
    configuration.psLedBrightness * 100.0, configuration.psLedBrightness * 100.0, 
    configuration.psStartHour, 
    configuration.psEndHour
  );

}

bool CWifiManager::updateConfigFromJson(JsonDocument jsonObj, bool allowHardware, String *error) {

  // Work on a copy so a rejected request leaves the running configuration untouched
  configuration_t c = configuration;
  bool networkChanged = false;

  // Device information
  if (!jsonObj["name"].isNull()) {
    Log.traceln("Setting 'name' to %s", jsonObj["name"].as<const char*>());
    strncpy(c.name, jsonObj["name"].as<const char*>(), sizeof(c.name) - 1);
    c.name[sizeof(c.name) - 1] = '\0';
  }

  if (!jsonObj["ledEnabled"].isNull()) {
    c.ledEnabled = jsonObj["ledEnabled"].as<bool>();
    Log.traceln("Setting 'ledEnabled' to %d", c.ledEnabled);
  }

  #ifdef WIFI
  // WiFi settings
  if (!jsonObj["wifiSsid"].isNull()) {
    strncpy(c.wifiSsid, jsonObj["wifiSsid"].as<const char*>(), sizeof(c.wifiSsid) - 1);
    c.wifiSsid[sizeof(c.wifiSsid) - 1] = '\0';
    Log.traceln("Setting 'wifiSsid' to %s", c.wifiSsid);
    networkChanged = true;
  }

  if (!jsonObj["wifiPassword"].isNull()) {
    strncpy(c.wifiPassword, jsonObj["wifiPassword"].as<const char*>(), sizeof(c.wifiPassword) - 1);
    c.wifiPassword[sizeof(c.wifiPassword) - 1] = '\0';
    Log.traceln("Setting 'wifiPassword'");
    networkChanged = true;
  }

  if (!jsonObj["wifiPower"].isNull()) {
    c.wifiPower = jsonObj["wifiPower"].as<int8_t>();
    Log.traceln("Setting 'wifiPower' to %d", c.wifiPower);
    networkChanged = true;
  }

  // NTP settings
  if (!jsonObj["ntpServer"].isNull()) {
    strncpy(c.ntpServer, jsonObj["ntpServer"].as<const char*>(), sizeof(c.ntpServer) - 1);
    c.ntpServer[sizeof(c.ntpServer) - 1] = '\0';
    Log.traceln("Setting 'ntpServer' to %s", c.ntpServer);
    networkChanged = true;
  }

  if (!jsonObj["gmtOffset_sec"].isNull()) {
    c.gmtOffset_sec = jsonObj["gmtOffset_sec"].as<long>();
    Log.traceln("Setting 'gmtOffset_sec' to %l", c.gmtOffset_sec);
    networkChanged = true;
  }

  if (!jsonObj["daylightOffset_sec"].isNull()) {
    c.daylightOffset_sec = jsonObj["daylightOffset_sec"].as<int>();
    Log.traceln("Setting 'daylightOffset_sec' to %d", c.daylightOffset_sec);
    networkChanged = true;
  }
  #endif

  #ifdef LED
  // LED hardware - applied as a whole after a save and reboot, so validated as a whole
  if (allowHardware) {
    if (!jsonObj["ledLayout"].isNull()) {
      int layout = jsonObj["ledLayout"].is<const char*>()
        ? CONFIG_ledLayoutFromName(jsonObj["ledLayout"].as<const char*>())
        : jsonObj["ledLayout"].as<int>();
      if (layout < 0 || layout >= LED_LAYOUT_COUNT) {
        if (error) *error = "unknown ledLayout";
        return false;
      }
      c.ledLayout = layout;
    }

    if (!jsonObj["ledColorOrder"].isNull()) {
      int order = CONFIG_ledColorOrderFromName(jsonObj["ledColorOrder"].as<const char*>());
      if (order < 0) {
        if (error) *error = "unknown ledColorOrder";
        return false;
      }
      c.ledColorOrder = order;
    }

    if (!jsonObj["ledType"].isNull()) c.ledType = jsonObj["ledType"].as<uint8_t>();
    if (!jsonObj["ledPin"].isNull()) c.ledPin = jsonObj["ledPin"].as<uint8_t>();
    if (!jsonObj["ledPin2"].isNull()) c.ledPin2 = jsonObj["ledPin2"].as<uint8_t>();
    if (!jsonObj["ledStripSize"].isNull()) c.ledStripSize = jsonObj["ledStripSize"].as<uint16_t>();
    if (!jsonObj["ledStripSize2"].isNull()) c.ledStripSize2 = jsonObj["ledStripSize2"].as<uint16_t>();
    if (!jsonObj["ledRingOuterSize"].isNull()) c.ledRingOuterSize = jsonObj["ledRingOuterSize"].as<uint16_t>();
    if (!jsonObj["ledMirror"].isNull()) c.ledMirror = jsonObj["ledMirror"].as<bool>();

    const char *ledError = CONFIG_checkLedHardware(c);
    if (ledError) {
      Log.warningln("Rejected LED hardware configuration: %s", ledError);
      if (error) *error = ledError;
      return false;
    }
  }

  // Modes must fit the layout they will run on: the requested one if this request changes it
  const uint8_t modeLayout = allowHardware ? c.ledLayout : ledLayout;

  // LED settings
  if (!jsonObj["ledBrightness"].isNull()) {
    float brightness = jsonObj["ledBrightness"].as<float>();
    if (brightness >= 0.0 && brightness <= 1.0) {
      c.ledBrightness = brightness;
      Log.traceln("Setting 'ledBrightness' to %D", c.ledBrightness);
    }
  }

  if (!jsonObj["ledMode"].isNull()) {
    uint8_t mode = jsonObj["ledMode"].as<uint8_t>();
    if (isModeSelectable(mode, modeLayout)) {
      c.ledMode = mode;
      Log.traceln("Setting 'ledMode' to %d", c.ledMode);
    }
  }

  if (!jsonObj["ledDelayMs"].isNull()) {
    c.ledDelayMs = jsonObj["ledDelayMs"].as<unsigned long>();
    Log.traceln("Setting 'ledDelayMs' to %l", c.ledDelayMs);
  }

  if (!jsonObj["ledCycleModeMs"].isNull()) {
    c.ledCycleModeMs = jsonObj["ledCycleModeMs"].as<unsigned long>();
    Log.traceln("Setting 'ledCycleModeMs' to %l", c.ledCycleModeMs);
  }

  if (!jsonObj["psLedBrightness"].isNull()) {
    float psBrightness = jsonObj["psLedBrightness"].as<float>();
    if (psBrightness >= 0.0 && psBrightness <= 1.0) {
      c.psLedBrightness = psBrightness;
      Log.traceln("Setting 'psLedBrightness' to %D", c.psLedBrightness);
    }
  }

  if (!jsonObj["psStartHour"].isNull()) {
    int8_t hour = jsonObj["psStartHour"].as<int8_t>();
    if (hour >= 0 && hour <= 23) {
      c.psStartHour = hour;
      Log.traceln("Setting 'psStartHour' to %d", c.psStartHour);
    }
  }

  if (!jsonObj["psEndHour"].isNull()) {
    int8_t hour = jsonObj["psEndHour"].as<int8_t>();
    if (hour >= 0 && hour <= 23) {
      c.psEndHour = hour;
      Log.traceln("Setting 'psEndHour' to %d", c.psEndHour);
    }
  }

  // Mode cycling configuration - a list wins over a bare count, and modes that don't fit are dropped
  if (!jsonObj["cycleModesList"].isNull() && jsonObj["cycleModesList"].is<JsonArray>()) {
    JsonArray modeList = jsonObj["cycleModesList"].as<JsonArray>();
    uint8_t count = 0;
    for (JsonVariant v : modeList) {
      uint8_t modeIndex = v.as<uint8_t>();
      if (count < 32 && isModeSelectable(modeIndex, modeLayout)) {
        c.cycleModesList[count++] = modeIndex;
      }
    }
    c.cycleModesCount = count;
    Log.traceln("Setting 'cycleModesList' with %d modes", c.cycleModesCount);
  } else if (!jsonObj["cycleModesCount"].isNull()) {
    uint8_t count = jsonObj["cycleModesCount"].as<uint8_t>();
    if (count <= 32) {
      c.cycleModesCount = count;
      Log.traceln("Setting 'cycleModesCount' to %d", c.cycleModesCount);
    }
  }
  #endif

  configuration = c;
  if (networkChanged) {
    rebootNeeded = true;
  }

  #ifdef LED
  // Update LED brightness calculation if power-save settings changed
  CONFIG_getLedBrightness(true);
  #endif

  return true;
}
