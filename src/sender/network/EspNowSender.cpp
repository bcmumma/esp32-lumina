#include "EspNowSender.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include "WiFiManager.h"
#include "../config/ConfigManager.h"
#include "CellFrame.h"
#include "Heartbeat.h"

// Phase 16: the room receivers' heartbeats (the only packets the S3 receives over ESP-NOW).
static portMUX_TYPE roomMux = portMUX_INITIALIZER_UNLOCKED;
static heartbeat::RoomStatus roomStatus[heartbeat::kRooms];

// Phase 16: an update-mode command for one room, repeated over the next few send slots.
static uint8_t commandRoom = 0, commandRepeats = 0;
static uint16_t commandNonce = 0;

void request_room_update(uint8_t room) {
  if (room < 1 || room > heartbeat::kRooms) return;
  commandRoom = room;
  commandNonce++;
  commandRepeats = 10;  // about a third of a second of send slots; the receiver acts once
}

static void on_espnow_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  (void)info;
  const uint32_t now = millis();
  portENTER_CRITICAL(&roomMux);
  heartbeat::accept(roomStatus, data, len, now);
  portEXIT_CRITICAL(&roomMux);
}

void append_rooms_json(String &s) {
  heartbeat::RoomStatus copy[heartbeat::kRooms];
  portENTER_CRITICAL(&roomMux);
  memcpy(copy, roomStatus, sizeof(copy));
  portEXIT_CRITICAL(&roomMux);
  const uint32_t now = millis();
  s += "[";
  for (uint8_t i = 0; i < heartbeat::kRooms; i++) {
    const heartbeat::RoomStatus &r = copy[i];
    if (i) s += ",";
    s += "{\"room\":" + String(i + 1) + ",\"seen\":" + String(r.seen ? 1 : 0) + ",\"online\":" + String(heartbeat::online(r, now) ? 1 : 0);
    if (r.seen) {
      s += ",\"ageMs\":" + String(now - r.lastMs) + ",\"lossPermille\":" + String(r.lossPermille) + ",\"fps\":" + String(r.last.fps);
      s += ",\"count\":[" + String(r.last.count[0]) + "," + String(r.last.count[1]) + "],\"rssi\":" + String(r.last.rssi);
      s += ",\"channel\":" + String(r.last.channel) + ",\"uptimeS\":" + String(r.last.uptimeMs / 1000UL);
      s += ",\"good\":" + String(r.last.good) + ",\"lost\":" + String(r.last.lost) + ",\"restarts\":" + String(r.restarts);
      s += ",\"updating\":" + String((r.last.flags & HEARTBEAT_FLAG_UPDATING) ? 1 : 0) + ",\"wifi\":" + String((r.last.flags & HEARTBEAT_FLAG_WIFI) ? 1 : 0);
      if (r.last.ip) s += ",\"ip\":\"" + IPAddress(r.last.ip).toString() + "\"";
    }
    s += "}";
  }
  s += "]";
}

static uint8_t pendingCells[MAX_CELLS * 3];
static uint8_t pendingCellCount = 0;
static uint16_t cellFrameId = 0;
uint32_t cell_packets_sent = 0;

void queue_cell_frame(const uint8_t *rgb, uint8_t total) {
  if (total > MAX_CELLS) total = MAX_CELLS;
  if (total) memcpy(pendingCells, rgb, total * 3U);
  pendingCellCount = total;
}

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
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok = send_cb_ok + 1;
  else send_cb_fail = send_cb_fail + 1;
}
#else
void on_espnow_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  espnow_busy = false;
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok = send_cb_ok + 1;
  else send_cb_fail = send_cb_fail + 1;
}
#endif

// Packets queued per second, measured once per second from loop() so the
// serial log and /api/* agree (it used to update only inside the HTTP handlers).
void update_packet_rate(uint32_t now) {
  if (now - last_pps_ms < 1000) return;
  uint32_t current = send_enqueue_ok;
  packets_per_sec = current - last_pps_count;
  last_pps_count = current;
  last_pps_ms = now;
}

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
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  p.senderChannel = (esp_wifi_get_channel(&primary, &second) == ESP_OK) ? primary : 0;
  esp_err_t err = esp_now_send(BROADCAST_ADDR, (const uint8_t *)&p, sizeof(p));

  if (err == ESP_OK) {
    sequence_num++;
    send_enqueue_ok++;
    forceEspNowSend = false;
    if (commandRepeats) {  // Phase 16: update-mode command for one room
      CommandPacket c = {COMMAND_MAGIC, COMMAND_VERSION, commandRoom, COMMAND_UPDATE_MODE, 0, commandNonce};
      if (esp_now_send(BROADCAST_ADDR, (const uint8_t *)&c, sizeof(c)) == ESP_OK) commandRepeats--;
    }
    if (pendingCellCount) {  // cell colours ride in the same slot, right behind the state packet
      CellPacket parts[2];
      uint16_t bytes[2];
      const uint8_t n = cells::encode(pendingCells, pendingCellCount, cellFrameId++, parts, bytes, 2);
      for (uint8_t k = 0; k < n; k++) {
        if (esp_now_send(BROADCAST_ADDR, (const uint8_t *)&parts[k], bytes[k]) == ESP_OK) cell_packets_sent++;
      }
    }
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
  heartbeat::reset(roomStatus);
  esp_now_register_recv_cb(on_espnow_recv);  // Phase 16: room heartbeats

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
