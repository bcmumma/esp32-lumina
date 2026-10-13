#include "EspNowSender.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include "WiFiManager.h"
#include "../config/ConfigManager.h"

// ESP-NOW broadcast. Your LED receiver must be on the same Wi-Fi channel
// as the router channel printed by this sender.
static const uint8_t BROADCAST_ADDR[6] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

uint16_t sequence_num = 0;
// Set when a browser command should be pushed on the very next audio block
// instead of waiting for the normal packet interval.
volatile bool forceEspNowSend = false;

volatile bool espnow_busy = false;
volatile uint32_t send_cb_ok = 0;
volatile uint32_t send_cb_fail = 0;

uint32_t send_enqueue_ok = 0;
uint32_t send_enqueue_err = 0;
uint32_t send_skipped_busy = 0;
uint32_t last_send_err = 0;
uint32_t last_send_ms = 0;
uint32_t busy_since_ms = 0;

uint32_t last_pps_ms = 0;
uint32_t last_pps_count = 0;
uint32_t packets_per_sec = 0;

// // ESP-NOW send callback
// Supports older and newer Arduino-ESP32 cores.
// ============================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void on_espnow_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  espnow_busy = false;
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok++;
  else send_cb_fail++;
}
#else
void on_espnow_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  espnow_busy = false;
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok++;
  else send_cb_fail++;
}
#endif

void maybe_send_espnow(AudioLedPacket &p) {
  uint32_t now = millis();
  bool forced = forceEspNowSend;

  if (!forced && now - last_send_ms < cfg.packetIntervalMs) return;

  if (espnow_busy) {
    if (now - busy_since_ms < 100) {
      send_skipped_busy++;
      return;
    }
    espnow_busy = false;
  }

  espnow_busy = true;
  busy_since_ms = now;
  last_send_ms = now;

  // Sequence numbers now identify transmitted packets, not every audio block.
  // This makes receiver loss statistics meaningful.
  p.sequence = sequence_num;
  esp_err_t err = esp_now_send(BROADCAST_ADDR, (const uint8_t *)&p, sizeof(p));

  if (err == ESP_OK) {
    sequence_num++;
    send_enqueue_ok++;
    forceEspNowSend = false;
  } else {
    espnow_busy = false;
    send_enqueue_err++;
    last_send_err = (uint32_t)err;
  }
}

void setup_espnow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
    while (true) delay(1000);
  }

  esp_now_register_send_cb(on_espnow_sent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
  // Channel 0 tells ESP-NOW to use the station's current router channel.
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;

  esp_err_t add_result = esp_now_add_peer(&peer);
  if (add_result != ESP_OK && add_result != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("Failed to add broadcast peer. err=");
    Serial.println((uint32_t)add_result);
    status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
    while (true) delay(1000);
  }

  Serial.println("ESP-NOW sender ready");
}
