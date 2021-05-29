#ifndef ESP8266
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <ezTime.h>


#include "wifi/WifiManager.h"
#include "Configuration.h"

#define MAX_CONNECT_TIMEOUT_MS 10000 // 10 seconds to connect before creating its own AP
#define BOARD_LED_PIN 2

const int RSSI_MAX =-50;// define maximum straighten of signal in dBm
const int RSSI_MIN =-100;// define minimum strength of signal in dBm

WiFiClient espClient;

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

CWifiManager::CWifiManager() {    
  pinMode(BOARD_LED_PIN,OUTPUT);
  strcpy(SSID, configuration.wifiSsid);

  connect();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s'", SSID);
    WiFi.begin(SSID, configuration.wifiPassword);
    
  } else {

    // Create AP using fallback and chip ID
    uint32_t chipId = 0;
    #ifdef ESP32
      for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
    #elif ESP8266
      chipId = ESP.getChipId();
    #endif
    

    Log.infoln("Chip ID: '%i'", chipId);

    sprintf_P(softAP_SSID, "%s_%i", WIFI_FALLBACK_SSID, chipId);
    Log.infoln("Creating WiFi: '%s'", softAP_SSID);
    
    WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS);

  }
  
}

void CWifiManager::listen() {

  status = WF_LISTENING;

  // Web
  server.on("/", std::bind(&CWifiManager::handleRoot, this));
  server.on("/connect", HTTP_POST, std::bind(&CWifiManager::handleConnect, this));
  //server.on("/led/external/matrix", HTTP_POST, std::bind(&CWifiManager::handleLEDMatrix, this));
  server.begin(WEB_SERVER_PORT);
  Log.infoln("Web server listening on port %i", WEB_SERVER_PORT);

  // NTP
  Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);

  configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
  struct tm timeinfo;
  
  /*
  //time()
  if(getLocalTime(&timeinfo)){
    Log.infoln("%i:%i", timeinfo.tm_hour,timeinfo.tm_min);
  }
  */
  
}

void CWifiManager::loop() {
  if (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_NO_SHIELD) {
    // WiFi is connected

    if (status == WF_LISTENING) {  
      // Handle requests
      server.handleClient();
    } else {
      // Start listening for requests
      listen();
    }

  } else {
    // WiFi is down

    switch (status) {
      case WF_LISTENING: {
        Log.infoln("Disconnecting");
        server.close();
        status = WF_CONNECTING;
        connect();
      } break;
      case WF_CONNECTING: {
        if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
          Log.infoln("Connecting failed, create an AP instead");
          esp_wifi_stop();

          tMillis = millis();
          strcpy(SSID, "");

          connect();
        }
      } break;

    }

  }
  
}

void CWifiManager::handleRoot() {
  digitalWrite(BOARD_LED_PIN, LOW);
  
  char temp[1000];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 1000,

           "<html>\
  <head>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #303030; font-family: 'Anaheim',sans-serif; Color: #d8d8d8; }\
    </style>\
  </head>\
  <body>\
    <h1>ProtoPuck32</h1>\
    <p>Connect to WiFi Access Point (AP)</p>\
    <form method='POST' action='/connect' enctype='application/x-www-form-urlencoded'>\
      <label for='ssid'>SSID (AP Name):</label><br>\
      <input type='text' id='ssid' name='ssid'><br><br>\
      <label for='pass'>Password (WPA2):</label><br>\
      <input type='password' id='pass' name='password' minlength='8' autocomplete='off' required><br><br>\
      <input type='submit' value='Connect...'>\
    </form>\
    <br><br><hr>\
    %s\
    <p>Uptime: %02d:%02d:%02d</p>\
  </body>\
</html>",
           getTempSensorResponse().c_str(),
           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", temp);

  digitalWrite(BOARD_LED_PIN, HIGH);
  
}

void CWifiManager::handleConnect() {
  digitalWrite(BOARD_LED_PIN, LOW);

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  char temp[1000];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 1000,

           "<html>\
  <head>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #303030; font-family: 'Anaheim',sans-serif; Color: #d8d8d8; }\
    </style>\
  </head>\
  <body>\
    <h1>ProtoPuck32</h1>\
    <p>Connecting to '%s' ... see you on the other side!</p>\
    <p>Uptime: %02d:%02d:%02d</p>\
  </body>\
</html>",
    ssid.c_str(),
    hr, min % 60, sec % 60
  );

  server.send(200, "text/html", temp);

  ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
  password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));

  log_i("Saved config SSID: '%s'", configuration.wifiSsid);

  EEPROM_saveConfig();

  strcpy(SSID, configuration.wifiSsid);
  connect();

  digitalWrite(BOARD_LED_PIN, HIGH);
  
}

String CWifiManager::getTempSensorResponse() {
#ifdef TEMP_SENSOR
  float temp = 12.3f;
  return String("<div>\
    Temperature: " + String(temp, 1) + "<br/>\
    Hunidity: TODO <br/>\
  </div>");
#else
  return "";
#endif
}

#ifdef LED_EXTERNAL_MATRIX
/*
  Post body should contain comma delimited items in a list like this: X Y RRGGBB,X Y RRGGBB
  Each item is a space delimited X,Y coordinates of the LED pixel in the matrix and the expected color in Hex
  Ex: 0 0 ffbbdd,1 0 c91232
*/
void CWifiManager::handleLEDMatrix() {
  digitalWrite(BOARD_LED_PIN, LOW);

  matrix_pixel_t pixels[LED_EXTERNAL_MATRIX_WIDTH * LED_EXTERNAL_MATRIX_HEIGHT];

  String postBody = server.arg("plain");
  log_d("LED Matrinx: %s", postBody.c_str());

  ioTManager->setLeds(pixels);

/*
  const char pixelDelimiter[1] = ",";
  const char *raw = postBody.c_str();
  char *pixelToken = strtok(raw, pixelDelimiter);

  while( pixelToken != NULL ) {
    log_d("P: '%s'", pixelToken);
    pixelToken = strtok(NULL, pixelDelimiter);
  }
*/
  
  //postBody.indexOf()
  

  
  server.send(200, "text/html", "OK");
  digitalWrite(BOARD_LED_PIN, HIGH);
}
#endif