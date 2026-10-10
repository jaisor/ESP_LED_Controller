// Partition migrator - a one-off firmware, built by the *_migrator environments.
//
// Moves an ESP32 / ESP32-C3 / ESP32-S3 with 4MB flash from the framework's default.csv (two 1.25MB
// app slots) to min_spiffs.csv (two 1.9MB slots) over the air, since a normal OTA update never
// rewrites the partition table and the current firmware no longer fits a 1.25MB slot.
//
//   1. Upload this firmware with the old firmware's /update page. It lands in app0 or app1.
//   2. If it booted from app1 it copies itself to app0 and reboots: app0 starts at 0x10000 in both
//      tables, so it keeps running from there after the switch. Only app1 and what follows move.
//   3. "Migrate" on its web page writes the new table to 0x8000 and reboots.
//   4. Its web page then takes the real firmware.bin, which goes into the new 1.9MB app1.
//
// nvs (the saved configuration) and otadata sit at the same offsets in both tables and are left
// alone. The table is only written when the current one is recognized as default.csv, and the
// write is read back and the old table put back if it doesn't match. A power cut during the
// fraction of a second the write takes still leaves a board that needs USB.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <MD5Builder.h>
#include <nvs.h>
#include <esp_flash.h>
#include <esp_flash_internal.h>
#include <esp_flash_partitions.h>
#include <esp_flash_encrypt.h>
#include <esp_secure_boot.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "Configuration.h"
#include "min_spiffs_table.h"

#define MIN_SPIFFS_TABLE_MD5_OFFSET 192  // MD5 entry follows the 6 partition entries
#define WIFI_CONNECT_TIMEOUT_MS 20000

enum table_layout_t { TABLE_UNKNOWN, TABLE_DEFAULT, TABLE_MIN_SPIFFS };

static WebServer server(WEB_SERVER_PORT);
alignas(4) static uint8_t tableSector[ESP_PARTITION_TABLE_SIZE];  // Partition table as read at boot
static table_layout_t layout = TABLE_UNKNOWN;
static String tableProblem;     // Why the layout isn't recognized
static String blockers;         // Why the table can't be migrated, one <li> per reason
static String lastError;
static bool uploadAccepted = false;

static const char *LAYOUT_NAMES[] = {"not recognized", "default.csv (1.25MB app slots)", "min_spiffs.csv (1.9MB app slots)"};

// ---------------------------------------------------------------------------- partition table

static bool entryMatches(const esp_partition_info_t &e, uint8_t type, uint8_t subtype, uint32_t offset, uint32_t size) {
  return e.type == type && e.subtype == subtype && e.pos.offset == offset && e.pos.size == size;
}

static table_layout_t classifyTable(const uint8_t *table, String &problem) {
  if (!memcmp(table, MIN_SPIFFS_TABLE, sizeof(MIN_SPIFFS_TABLE))) {
    return TABLE_MIN_SPIFFS;
  }

  // default.csv: nvs, otadata, app0, app1 exactly as below. What follows app1 (spiffs, coredump) is
  // data that min_spiffs reuses, and older framework versions laid it out differently, so it only
  // has to stay above the end of app1.
  const esp_partition_info_t *entries = (const esp_partition_info_t*)table;
  bool nvs = false, otadata = false, app0 = false, app1 = false;
  for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES && entries[i].magic == ESP_PARTITION_MAGIC; i++) {
    const esp_partition_info_t &e = entries[i];
    char labelBuffer[sizeof(e.label) + 1] = {};  // Labels aren't terminated when they use all 16 bytes
    memcpy(labelBuffer, e.label, sizeof(e.label));
    String label = labelBuffer;
    if (label == "nvs" && entryMatches(e, PART_TYPE_DATA, 0x02, 0x9000, 0x5000)) nvs = true;
    else if (label == "otadata" && entryMatches(e, PART_TYPE_DATA, PART_SUBTYPE_DATA_OTA, 0xE000, 0x2000)) otadata = true;
    else if (label == "app0" && entryMatches(e, PART_TYPE_APP, 0x10, 0x10000, 0x140000)) app0 = true;
    else if (label == "app1" && entryMatches(e, PART_TYPE_APP, 0x11, 0x150000, 0x140000)) app1 = true;
    else if (e.type == PART_TYPE_APP || e.pos.offset < 0x290000) {
      problem = String("unexpected partition '") + label + "'";
      return TABLE_UNKNOWN;
    }
  }
  if (!(nvs && otadata && app0 && app1)) {
    problem = "nvs, otadata, app0 or app1 differ from default.csv";
    return TABLE_UNKNOWN;
  }
  return TABLE_DEFAULT;
}

static String describeTable(const uint8_t *table) {
  String s;
  const esp_partition_info_t *entries = (const esp_partition_info_t*)table;
  for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES && entries[i].magic == ESP_PARTITION_MAGIC; i++) {
    char line[96];
    snprintf(line, sizeof(line), "%-9.16s type %u/0x%02x  0x%06x  %4u KB\n", (const char*)entries[i].label,
      entries[i].type, entries[i].subtype, (unsigned)entries[i].pos.offset, (unsigned)(entries[i].pos.size / 1024));
    s += line;
  }
  return s;
}

static bool embeddedTableValid() {
  MD5Builder md5;
  md5.begin();
  md5.add(MIN_SPIFFS_TABLE, MIN_SPIFFS_TABLE_MD5_OFFSET);
  md5.calculate();
  uint8_t digest[16];
  md5.getBytes(digest);
  const uint8_t *entry = MIN_SPIFFS_TABLE + MIN_SPIFFS_TABLE_MD5_OFFSET;
  return entry[0] == 0xEB && entry[1] == 0xEB && !memcmp(digest, entry + ESP_PARTITION_MD5_OFFSET, sizeof(digest));
}

// Erase and write the partition table sector, read it back. The IDF refuses writes below the first
// partition unless its OS-level protection is lifted for the duration.
static esp_err_t writeTableSector(const uint8_t *sector) {
  alignas(4) static uint8_t verify[ESP_PARTITION_TABLE_SIZE];
  esp_err_t err = esp_flash_erase_region(NULL, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
  if (err == ESP_OK) err = esp_flash_write(NULL, sector, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
  if (err == ESP_OK) err = esp_flash_read(NULL, verify, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
  if (err == ESP_OK && memcmp(verify, sector, ESP_PARTITION_TABLE_SIZE)) err = ESP_ERR_INVALID_CRC;
  return err;
}

static bool migrateTable(String &error) {
  alignas(4) static uint8_t sector[ESP_PARTITION_TABLE_SIZE];
  memset(sector, 0xFF, sizeof(sector));
  memcpy(sector, MIN_SPIFFS_TABLE, sizeof(MIN_SPIFFS_TABLE));

  Serial.println("Writing min_spiffs partition table");
  esp_flash_app_disable_protect(true);
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < 3 && err != ESP_OK; attempt++) {
    err = writeTableSector(sector);
    Serial.printf("  attempt %d: %s\n", attempt + 1, esp_err_to_name(err));
    if (err == ESP_ERR_NOT_ALLOWED || err == ESP_ERR_NOT_SUPPORTED) break;  // Refused before erasing anything
  }
  bool restored = false;
  if (err != ESP_OK && err != ESP_ERR_NOT_ALLOWED && err != ESP_ERR_NOT_SUPPORTED) {
    // Put the table the device booted with back, so it still boots the way it did
    for (int attempt = 0; attempt < 3 && !restored; attempt++) {
      restored = writeTableSector(tableSector) == ESP_OK;
    }
  }
  esp_flash_app_disable_protect(false);

  if (err == ESP_OK) {
    return true;
  }
  error = String("writing the partition table failed: ") + esp_err_to_name(err);
  if (err == ESP_ERR_NOT_ALLOWED || err == ESP_ERR_NOT_SUPPORTED) {
    error += ". Nothing was changed.";
  } else if (restored) {
    error += ". The previous table was put back; the device still boots as before.";
  } else {
    error += ". RESTORING THE PREVIOUS TABLE FAILED TOO - do not reboot, the device will need USB to recover.";
  }
  return false;
}

// ---------------------------------------------------------------------------- app slot

// app1 moves in the new table, so the migrator has to run from app0 before rewriting it
static bool copySelfToApp0(String &error) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *app0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  if (!app0) {
    error = "no app0 partition";
    return false;
  }

  esp_partition_pos_t pos = { running->address, running->size };
  esp_image_metadata_t image;
  if (esp_image_get_metadata(&pos, &image) != ESP_OK) {
    error = "can't read the running image";
    return false;
  }

  Serial.printf("Copying %u byte image from %s to app0\n", (unsigned)image.image_len, running->label);
  esp_ota_handle_t handle;
  esp_err_t err = esp_ota_begin(app0, image.image_len, &handle);
  static uint8_t buffer[4096];
  for (size_t offset = 0; err == ESP_OK && offset < image.image_len; offset += sizeof(buffer)) {
    size_t length = min(sizeof(buffer), (size_t)(image.image_len - offset));
    err = esp_partition_read(running, offset, buffer, length);
    if (err == ESP_OK) err = esp_ota_write(handle, buffer, length);
  }
  if (err == ESP_OK) err = esp_ota_end(handle);
  if (err == ESP_OK) err = esp_ota_set_boot_partition(app0);
  if (err != ESP_OK) {
    error = String("copying to app0 failed: ") + esp_err_to_name(err);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------- WiFi

// Reads the main firmware's saved WiFi network without writing anything: the EEPROM library lives in
// the NVS blob "eeprom", configuration_t starts at EEPROM_CONFIGURATION_START
static bool readSavedWifi(String &ssid, String &password) {
  nvs_handle_t handle;
  if (nvs_open("eeprom", NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }
  size_t length = 0;
  bool ok = false;
  if (nvs_get_blob(handle, "eeprom", NULL, &length) == ESP_OK
      && length >= EEPROM_CONFIGURATION_START + offsetof(configuration_t, _loaded) + sizeof(configuration_t::_loaded)) {
    uint8_t *blob = (uint8_t*)malloc(length);
    if (blob && nvs_get_blob(handle, "eeprom", blob, &length) == ESP_OK) {
      const uint8_t *config = blob + EEPROM_CONFIGURATION_START;
      char loaded[sizeof(configuration_t::_loaded) + 1] = {};
      memcpy(loaded, config + offsetof(configuration_t, _loaded), sizeof(configuration_t::_loaded));
      if (!strcmp(loaded, "jaisor")) {
        char value[sizeof(configuration_t::wifiPassword) + 1] = {};
        memcpy(value, config + offsetof(configuration_t, wifiSsid), sizeof(configuration_t::wifiSsid));
        ssid = value;
        memset(value, 0, sizeof(value));
        memcpy(value, config + offsetof(configuration_t, wifiPassword), sizeof(configuration_t::wifiPassword));
        password = value;
        ok = ssid.length() > 0;
      }
    }
    free(blob);
  }
  nvs_close(handle);
  return ok;
}

static void startWifi() {
  String ssid, password;
  if (readSavedWifi(ssid, password)) {
    Serial.printf("Connecting to '%s'\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected, open http://%s/\n", WiFi.localIP().toString().c_str());
      return;
    }
    Serial.println("Connecting failed");
  }
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String apName = String(DEVICE_NAME) + "_" + chipId + "_migrator";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), WIFI_FALLBACK_PASS);
  Serial.printf("Access point '%s' / '%s', open http://%s/\n", apName.c_str(), WIFI_FALLBACK_PASS, WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------------------- web

static String pageTop() {
  return F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Partition migrator</title><style>body{font-family:sans-serif;max-width:46rem;margin:1rem auto;padding:0 1rem;line-height:1.4}"
    "pre{background:#eee;padding:.5rem;overflow-x:auto}.err{color:#b00}button{font-size:1rem;padding:.5rem 1rem}</style></head><body>"
    "<h2>ESP LED Controller - partition migrator</h2>");
}

static void handleStatus() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  uint32_t flashSize = 0;
  esp_flash_get_size(NULL, &flashSize);

  String page = pageTop();
  page += "<p>Chip <b>" + String(ESP.getChipModel()) + "</b>, " + (flashSize / (1024 * 1024)) + " MB flash, running from <b>"
    + running->label + "</b> at 0x" + String(running->address, HEX) + "</p>";
  page += String("<p>Partition table: <b>") + LAYOUT_NAMES[layout] + "</b>"
    + (tableProblem.length() ? " - " + tableProblem : "") + "</p><pre>" + describeTable(tableSector) + "</pre>";
  if (lastError.length()) {
    page += "<p class='err'>" + lastError + "</p>";
  }

  if (layout == TABLE_MIN_SPIFFS) {
    page += F("<h3>Step 2 of 2: upload the firmware</h3>"
      "<p>The partition table is migrated. Upload the ESP LED Controller <code>firmware.bin</code> built for this board "
      "(<code>.pio/build/&lt;env&gt;/firmware.bin</code>). It replaces this migrator and keeps the saved settings.</p>"
      "<form method='POST' action='update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware' accept='.bin' required> <button type='submit'>Upload and reboot</button></form>");
  } else if (layout == TABLE_DEFAULT && !blockers.length()) {
    page += F("<h3>Step 1 of 2: migrate the partition table</h3>"
      "<p>Writes <code>min_spiffs.csv</code> to the partition table and reboots into this migrator. Settings are kept. "
      "The write takes a fraction of a second; a power cut during it leaves a board that needs USB to recover.</p>"
      "<form method='POST' action='migrate' onsubmit=\"return confirm('Rewrite the partition table now?')\">"
      "<button type='submit'>Migrate partition table</button></form>");
  } else {
    page += "<h3>Not migrating</h3><ul>" + blockers + "</ul><p>The partition table was not touched. Flash this board over USB instead.</p>";
  }
  page += F("</body></html>");
  server.send(200, "text/html; charset=utf-8", page);
}

static void handleMigrate() {
  if (layout != TABLE_DEFAULT || blockers.length()) {
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }
  String error;
  if (!migrateTable(error)) {
    lastError = error;
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }
  server.send(200, "text/html; charset=utf-8", pageTop() +
    F("<p>Partition table migrated. Rebooting - reload this page in about 30 seconds to upload the firmware.</p></body></html>"));
  delay(500);
  ESP.restart();
}

static void handleUploadDone() {
  if (uploadAccepted && !Update.hasError()) {
    server.send(200, "text/html; charset=utf-8", pageTop() +
      F("<p>Firmware uploaded. Rebooting into it - the controller's own page is back in about 30 seconds.</p></body></html>"));
    delay(500);
    ESP.restart();
  } else {
    lastError = uploadAccepted ? String("upload failed: ") + Update.errorString() : String("upload refused: partition table not migrated");
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

static void handleUploadData() {
  HTTPUpload &upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      uploadAccepted = layout == TABLE_MIN_SPIFFS;
      if (uploadAccepted) {
        Serial.printf("Receiving %s\n", upload.filename.c_str());
        Update.begin(UPDATE_SIZE_UNKNOWN);
      }
      break;
    case UPLOAD_FILE_WRITE:
      if (uploadAccepted) Update.write(upload.buf, upload.currentSize);
      break;
    case UPLOAD_FILE_END:
      if (uploadAccepted) Update.end(true);
      Serial.printf("Upload done, %u bytes, %s\n", (unsigned)upload.totalSize, Update.hasError() ? Update.errorString() : "ok");
      break;
    case UPLOAD_FILE_ABORTED:
      if (uploadAccepted) Update.abort();
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------- setup / loop

static void addBlocker(const String &reason) {
  blockers += "<li>" + reason + "</li>";
  Serial.printf("Blocked: %s\n", reason.c_str());
}

void setup() {
  Serial.begin(SERIAL_MONITOR_BAUD);
  delay(500);
  Serial.println("\nPartition migrator");

  if (esp_flash_read(NULL, tableSector, ESP_PARTITION_TABLE_OFFSET, sizeof(tableSector)) != ESP_OK) {
    tableProblem = "can't read it";
  } else {
    layout = classifyTable(tableSector, tableProblem);
  }
  Serial.printf("Partition table: %s %s\n%s", LAYOUT_NAMES[layout], tableProblem.c_str(), describeTable(tableSector).c_str());

  const esp_partition_t *running = esp_ota_get_running_partition();
  uint32_t flashSize = 0;
  esp_flash_get_size(NULL, &flashSize);

  if (layout == TABLE_UNKNOWN) addBlocker("the partition table isn't the framework's default.csv");
  if (layout == TABLE_DEFAULT) {
    if (flashSize < 0x400000) addBlocker("min_spiffs.csv needs 4MB of flash");
    if (esp_flash_encryption_enabled()) addBlocker("flash encryption is enabled");
    if (esp_secure_boot_enabled()) addBlocker("secure boot is enabled");
    if (!embeddedTableValid()) addBlocker("the embedded partition table fails its MD5 check");

    if (!blockers.length() && running->address != 0x10000) {
      String error;
      if (copySelfToApp0(error)) {
        Serial.println("Rebooting into app0");
        delay(200);
        ESP.restart();
      }
      addBlocker(error);
    }
  }

  startWifi();
  server.on("/", HTTP_GET, handleStatus);
  server.on("/migrate", HTTP_POST, handleMigrate);
  server.on("/update", HTTP_POST, handleUploadDone, handleUploadData);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(303); });
  server.begin();
}

void loop() {
  server.handleClient();
  delay(2);
}
