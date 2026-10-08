// MQTT and Home Assistant integration for CWifiManager
//
// Topics, all under <mqttTopic>/<device id>:
//   availability  "online" / "offline" (last will), retained
//   state         Home Assistant JSON light state {"state", "brightness", "effect"}, retained
//   set           Home Assistant JSON light commands
//   config        Configuration JSON, same fields as POST /config; saved, reboots if needed
//   json          Telemetry for the diagnostic sensors, retained
// Discovery goes to <MQTT_DISCOVERY_PREFIX>/<component>/<device id>[_<entity>]/config.

#include <Arduino.h>
#include <StreamUtils.h>
#include <version.h>
#include <memory>
#include "Configuration.h"
#include "wifi/WifiManager.h"

int dBmtoPercentage(int dBm);

bool CWifiManager::isMqttConfigured() {
  return strlen(configuration.mqttServer) && strlen(configuration.mqttTopic);
}

String CWifiManager::mqttTopic(const char *suffix) {
  return mqttBaseTopic + "/" + suffix;
}

static uint8_t brightnessByte() {
  return (uint8_t)(configuration.ledBrightness * 255.0f + 0.5f);
}

void CWifiManager::mqttLoop() {
  if (mqttSavePending && millis() - tsMqttSave > MQTT_SAVE_DELAY_MS) {
    mqttSavePending = false;
    EEPROM_saveConfig();
  }

  if (!isMqttConfigured()) {
    return;
  }

  if (!mqtt.connected()) {
    mqttStatePublished = false;
    if (!tsMqttConnect || millis() - tsMqttConnect > MQTT_RECONNECT_MS) {
      tsMqttConnect = millis();
      mqttConnect();
    }
    mqttStateCode = mqtt.state();
    return;
  }

  mqtt.loop();
  mqttStateCode = mqtt.state();

  if (mqttDiscoveryNeeded) {
    mqttPublishDiscovery();
    mqttStatePublished = false;  // The effect name may have changed too
  }

  // Changes from the web UI, /api, mode cycling or Home Assistant itself all end up here
  if (!mqttStatePublished || mqttLastPower != configuration.ledPower
    || mqttLastBrightness != brightnessByte() || mqttLastMode != configuration.ledMode) {
    mqttPublishState();
  }

  if (millis() - tsMqttTelemetry > MQTT_TELEMETRY_MS) {
    mqttPublishTelemetry();
  }
}

void CWifiManager::mqttConnect() {
  String clientId = String(DEVICE_NAME) + "_" + CONFIG_getDeviceId();
  String availability = mqttTopic("availability");
  bool auth = strlen(configuration.mqttUser) > 0;

  Log.noticeln("Connecting to MQTT '%s:%u' as '%s'", configuration.mqttServer, configuration.mqttPort, clientId.c_str());
  mqtt.setServer(configuration.mqttServer, configuration.mqttPort);
  if (!mqtt.connect(clientId.c_str(),
      auth ? configuration.mqttUser : nullptr, auth ? configuration.mqttPassword : nullptr,
      availability.c_str(), 0, true, "offline")) {
    Log.warningln("MQTT connect failed, state %i, retrying in %i s", mqtt.state(), MQTT_RECONNECT_MS / 1000);
    return;
  }

  Log.noticeln("MQTT connected, topics under '%s'", mqttBaseTopic.c_str());
  mqtt.publish(availability.c_str(), "online", true);
  mqtt.subscribe(mqttTopic("set").c_str());
  mqtt.subscribe(mqttTopic("config").c_str());
  mqtt.subscribe(MQTT_DISCOVERY_PREFIX "/status");

  mqttPublishDiscovery();
  mqttPublishState();
  mqttPublishTelemetry();
}

void CWifiManager::mqttPublishJson(const String &topic, JsonDocument &doc, bool retain) {
  // Streamed, so messages aren't limited by the PubSubClient buffer
  if (!mqtt.beginPublish(topic.c_str(), measureJson(doc), retain)) {
    Log.warningln("MQTT publish to '%s' failed", topic.c_str());
    return;
  }
  BufferingPrint buffered(mqtt, 64);
  serializeJson(doc, buffered);
  buffered.flush();
  mqtt.endPublish();
}

// Home Assistant effects are the modes available on the running layout, by name. Names are made
// unique (custom modes can repeat one) since Home Assistant sends the name back.
void CWifiManager::mqttEffectNames(std::vector<std::pair<uint8_t, String>> &names) {
  for (uint8_t i = 0; modes != NULL && i < modes->size(); i++) {
    if (!isModeSelectable(i, ledLayout)) {
      continue;
    }
    String name = (*modes)[i]->getName();
    for (auto &existing : names) {
      if (existing.second == name) {
        name += String(" (") + i + ")";
        break;
      }
    }
    names.push_back({i, name});
  }
}

void CWifiManager::mqttPublishDiscovery() {
  mqttDiscoveryNeeded = false;

  String id = String(CONFIG_getDeviceId());
  String lightTopic = String(MQTT_DISCOVERY_PREFIX "/light/") + id + "/config";
  String rssiTopic = String(MQTT_DISCOVERY_PREFIX "/sensor/") + id + "_rssi/config";
  String ipTopic = String(MQTT_DISCOVERY_PREFIX "/sensor/") + id + "_ip/config";

  if (!configuration.mqttDiscovery) {
    // Remove whatever an earlier boot with discovery on left behind
    mqtt.publish(lightTopic.c_str(), "", true);
    mqtt.publish(rssiTopic.c_str(), "", true);
    mqtt.publish(ipTopic.c_str(), "", true);
    return;
  }

  auto addDevice = [&](JsonDocument &doc) {
    doc["availability_topic"] = mqttTopic("availability");
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = id;
    device["name"] = configuration.name;
    device["model"] = DEVICE_NAME;
    device["manufacturer"] = "Custom";
    device["sw_version"] = VERSION_SHORT;
    device["configuration_url"] = String("http://") + currentIP() + "/";
  };

  {
    JsonDocument doc;
    doc["name"] = nullptr;  // The light is the device's main entity, so it takes the device name
    doc["unique_id"] = id + "_light";
    doc["schema"] = "json";
    doc["command_topic"] = mqttTopic("set");
    doc["state_topic"] = mqttTopic("state");
    doc["brightness"] = true;
    doc["brightness_scale"] = 255;
    doc["supported_color_modes"][0] = "brightness";
    doc["effect"] = true;
    JsonArray effects = doc["effect_list"].to<JsonArray>();
    std::vector<std::pair<uint8_t, String>> names;
    mqttEffectNames(names);
    for (auto &name : names) {
      effects.add(name.second);
    }
    addDevice(doc);
    mqttPublishJson(lightTopic, doc, true);
  }

  {
    JsonDocument doc;
    doc["name"] = "WiFi signal";
    doc["unique_id"] = id + "_rssi";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "dBm";
    doc["entity_category"] = "diagnostic";
    doc["state_topic"] = mqttTopic("json");
    doc["value_template"] = "{{ value_json.wifi_rssi }}";
    addDevice(doc);
    mqttPublishJson(rssiTopic, doc, true);
  }

  {
    JsonDocument doc;
    doc["name"] = "IP address";
    doc["unique_id"] = id + "_ip";
    doc["icon"] = "mdi:ip-network";
    doc["entity_category"] = "diagnostic";
    doc["state_topic"] = mqttTopic("json");
    doc["value_template"] = "{{ value_json.ip }}";
    addDevice(doc);
    mqttPublishJson(ipTopic, doc, true);
  }

  Log.noticeln("Published Home Assistant discovery for device %s", id.c_str());
}

void CWifiManager::mqttPublishState() {
  JsonDocument doc;
  doc["state"] = configuration.ledPower ? "ON" : "OFF";
  doc["brightness"] = brightnessByte();
  doc["color_mode"] = "brightness";
  std::vector<std::pair<uint8_t, String>> names;
  mqttEffectNames(names);
  for (auto &name : names) {
    if (name.first == configuration.ledMode) {
      doc["effect"] = name.second;
      break;
    }
  }
  mqttPublishJson(mqttTopic("state"), doc, true);

  mqttLastPower = configuration.ledPower;
  mqttLastBrightness = brightnessByte();
  mqttLastMode = configuration.ledMode;
  mqttStatePublished = true;
}

void CWifiManager::mqttPublishTelemetry() {
  tsMqttTelemetry = millis();

  JsonDocument doc;
  doc["name"] = configuration.name;
  doc["device_id"] = CONFIG_getDeviceId();
  doc["version"] = VERSION;
  doc["ip"] = currentIP();
  doc["mac"] = WiFi.macAddress();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["wifi_percent"] = dBmtoPercentage(WiFi.RSSI());
  doc["uptime_millis"] = CONFIG_getUpTime();
  doc["led_power"] = (bool)configuration.ledPower;
  doc["led_brightness"] = configuration.ledBrightness;
  doc["led_mode"] = configuration.ledMode;
  if (modes != NULL && configuration.ledMode < modes->size()) {
    doc["led_mode_name"] = (*modes)[configuration.ledMode]->getName();
  }
  doc["led_layout"] = LED_LAYOUT_IDS[ledLayout];
  doc["command_topic"] = mqttTopic("set");
  doc["config_topic"] = mqttTopic("config");
  mqttPublishJson(mqttTopic("json"), doc, true);
}

void CWifiManager::mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  Log.verboseln("MQTT message on '%s', %u bytes", topic, length);

  if (!strcmp(topic, MQTT_DISCOVERY_PREFIX "/status")) {
    // Home Assistant (re)started - it may have lost retained discovery, so send it again
    if (length == 6 && !memcmp(payload, "online", 6)) {
      mqttDiscoveryNeeded = true;
    }
    return;
  }
  if (length == 0) {
    return;  // Includes our own clearing of a retained config message
  }

  // Copies the payload, which lives in the client buffer that the next publish reuses
  JsonDocument json;
  DeserializationError de = deserializeJson(json, (const uint8_t*)payload, length);
  if (de) {
    Log.errorln("Invalid JSON on MQTT topic '%s': %s", topic, de.c_str());
    return;
  }

  if (mqttTopic("set") == topic) {
    mqttHandleCommand(json);
  } else if (mqttTopic("config") == topic) {
    mqttHandleConfig(json);
  }
}

// Home Assistant JSON schema light command: {"state": "ON"|"OFF", "brightness": 0-255, "effect": "<mode>"}
void CWifiManager::mqttHandleCommand(JsonDocument &command) {
  const char *state = command["state"];
  if (state && !strcasecmp(state, "ON")) {
    configuration.ledPower = true;
    if (configuration.ledBrightness <= 0 && command["brightness"].isNull()) {
      configuration.ledBrightness = 1.0f;  // Turning on at zero brightness would look like nothing happened
    }
  } else if (state && !strcasecmp(state, "OFF")) {
    configuration.ledPower = false;
  }

  if (command["brightness"].is<int>()) {
    int brightness = constrain(command["brightness"].as<int>(), 0, 255);
    if (brightness == 0) {
      configuration.ledPower = false;
    } else {
      configuration.ledBrightness = brightness / 255.0f;
    }
  }

  const char *effect = command["effect"];
  if (effect) {
    std::vector<std::pair<uint8_t, String>> names;
    mqttEffectNames(names);
    bool found = false;
    for (auto &name : names) {
      if (name.second == effect) {
        configuration.ledMode = name.first;
        updateModeChangeTime();
        found = true;
        break;
      }
    }
    if (!found) {
      Log.warningln("Unknown effect '%s' from MQTT", effect);
    }
  }

  Log.noticeln("MQTT command: power %d, brightness %D, mode %d", configuration.ledPower, configuration.ledBrightness, configuration.ledMode);
  CONFIG_getLedBrightness(true);
  mqttSavePending = true;
  tsMqttSave = millis();
  mqttStatePublished = false;  // Home Assistant waits for the state to confirm, even if nothing changed
}

// Same as POST /config: validated as a whole, saved, and reboots for network or LED hardware changes
void CWifiManager::mqttHandleConfig(JsonDocument &json) {
  // Clear a retained message so it isn't applied again on every reconnect
  mqtt.publish(mqttTopic("config").c_str(), "", true);

  std::unique_ptr<configuration_t> before(new configuration_t(configuration));
  String error;
  if (!updateConfigFromJson(json, true, &error)) {
    Log.errorln("Rejected configuration from MQTT: %s", error.c_str());
    return;
  }
  Log.noticeln("Applied configuration from MQTT");
  EEPROM_saveConfig();
  mqttDiscoveryNeeded = true;
  if (!CONFIG_ledHardwareEquals(*before, configuration)) {
    tMillis = millis();
    rebootNeeded = true;
  }
}

String CWifiManager::mqttStatusHtml() {
  if (!isMqttConfigured() || isApMode()) {
    return "";
  }
  if (mqttStateCode == MQTT_CONNECTED) {
    return "<span>MQTT ✅ ▪ </span>";
  }
  return String("<span>MQTT <a href='mqtt_reconnect' title='Reconnect now'>❌<sup>") + mqttStateCode + "</sup></a> ▪ </span>";
}

void CWifiManager::handleMqttReconnect(AsyncWebServerRequest *request) {
  Log.traceln("handleMqttReconnect");
  tsMqttConnect = 0;  // loop() connects on its next pass
  request->redirect("/");
}
