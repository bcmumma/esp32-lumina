#include "UpdateMode.h"

#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "LuminaProtocol.h"

// Credentials live in the gitignored include/secrets.h (template: include/secrets.example.h).
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h to include/secrets.h and fill it in."
#endif
#ifndef LUMINA_OTA_PASSWORD
#error "include/secrets.h needs LUMINA_OTA_PASSWORD (see include/secrets.example.h)"
#endif

#ifndef LUMINA_ROOM
#define LUMINA_ROOM 1
#endif

static volatile bool requested = false;
static bool active = false;
static uint32_t startedMs = 0;
static WebServer *server = nullptr;
static HTTPUpdateServer updater;

bool update_mode_handle_command(const uint8_t *data, int len) {
  if (len != (int)sizeof(CommandPacket) || (uint16_t)(data[0] | (data[1] << 8)) != COMMAND_MAGIC) return false;
  CommandPacket c;
  memcpy(&c, data, sizeof(c));
  if (c.version == COMMAND_VERSION && c.room == LUMINA_ROOM && c.command == COMMAND_UPDATE_MODE) requested = true;
  return true;
}

void update_mode_loop(uint32_t nowMs) {
  if (requested && !active) {
    requested = false;
    active = true;
    startedMs = nowMs;
    char host[24];
    snprintf(host, sizeof(host), "lumina-room%u", (unsigned)LUMINA_ROOM);
    WiFi.setHostname(host);
    WiFi.begin(LUMINA_WIFI_SSID, LUMINA_WIFI_PASS);  // same AP (and channel) as the S3, so ESP-NOW keeps working
    server = new WebServer(80);
    updater.setup(server, "/update", "lumina", LUMINA_OTA_PASSWORD);
    server->on("/", []() { server->send(200, "text/plain", "Lumina room receiver: update mode. POST the firmware to /update."); });
    server->begin();
    Serial.printf("Update mode: joining Wi-Fi as %s for %lu min\n", host, (unsigned long)(kUpdateWindowMs / 60000UL));
  }
  if (!active) return;
  static bool announced = false;
  if (!announced && WiFi.status() == WL_CONNECTED) {
    announced = true;
    Serial.printf("Update mode: ready at http://%s/update\n", WiFi.localIP().toString().c_str());
  }
  server->handleClient();
  if (nowMs - startedMs > kUpdateWindowMs) {
    Serial.println("Update mode: no upload in time, restarting to normal");
    delay(100);
    ESP.restart();
  }
}

bool update_mode_active() { return active; }
bool update_mode_wifi_connected() { return active && WiFi.status() == WL_CONNECTED; }
uint32_t update_mode_ip() { return update_mode_wifi_connected() ? (uint32_t)WiFi.localIP() : 0; }
