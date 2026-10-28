#include "EspNowReceiver.h"
#include "UpdateMode.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

// In home-Wi-Fi mode, the sender must use the router's 2.4 GHz channel.
// This receiver automatically scans channels until it finds valid Lumina
// packets, locks to that channel, and resumes scanning if packets disappear.

volatile uint8_t currentWifiChannel = FIRST_WIFI_CHANNEL;
volatile uint8_t lockedWifiChannel = 0;
volatile bool wifiChannelLocked = false;
uint32_t lastChannelHopMs = 0;
uint8_t lastPrintedLockedChannel = 0;

// Phase 8 fix (docs/KNOWN_ISSUES.md): adjacent channels leak packets at short
// range, so the first packet doesn't lock. It starts a survey: one full sweep
// with one dwell per channel, then lock to the channel whose packets arrive
// strongest (leaked packets are weaker). Channels need 2+ packets to qualify.
volatile uint16_t channelGoodCount[LAST_WIFI_CHANNEL + 1] = {};
volatile int32_t channelRssiSum[LAST_WIFI_CHANNEL + 1] = {};
bool channelSurveying = false;
uint8_t surveyHopsLeft = 0;

portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
AudioLedPacket latestPacket = {};
uint8_t latestMac[6] = {};
volatile bool havePacket = false;
volatile uint32_t goodPackets = 0;
volatile uint32_t badPackets = 0;
volatile uint32_t packetGeneration = 0;
uint32_t lastPacketMs = 0;

// Channel the sender announces in each packet (0 = none yet / older sender).
volatile uint8_t announcedChannel = 0;

static void reset_channel_survey();

// Phase 15: SoundSwitch cell frames arrive as a second packet type (CellPacket).
cells::Frame cellFrame = {};
volatile uint32_t lastCellMs = 0;
volatile int8_t lastSenderRssi = 0;
// Phase 16: packets that never arrived, counted as they arrive (sequence gaps in the radio callback).
// The loop can also skip a packet that a newer one replaced before it was read; that is not loss.
volatile uint32_t radioLost = 0;
static uint16_t lastRxSequence = 0;
static bool haveRxSequence = false;  // RSSI of the last good packet from the S3 (Phase 16 heartbeat)
static const uint8_t HEARTBEAT_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void copy_packet_from_callback(const uint8_t *mac, const uint8_t *data, int len, int rssi) {
  if (update_mode_handle_command(data, len)) return;  // Phase 16: S3 -> receiver commands
  // The other room's heartbeat is broadcast too: not for us, and not an error.
  if (len == sizeof(HeartbeatPacket) && (uint16_t)(data[0] | (data[1] << 8)) == HEARTBEAT_MAGIC) return;
  if (len != sizeof(AudioLedPacket) && len >= 2 && (uint16_t)(data[0] | (data[1] << 8)) == CELL_PACKET_MAGIC) {
    portENTER_CRITICAL(&packetMux);
    bool ok = cells::accept(cellFrame, data, len);
    portEXIT_CRITICAL(&packetMux);
    if (ok) lastCellMs = millis(); else badPackets = badPackets + 1;
    return;
  }
  if (len != sizeof(AudioLedPacket)) {
    badPackets = badPackets + 1;
    return;
  }

  AudioLedPacket p = {};
  memcpy(&p, data, sizeof(p));

  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION || p.size != sizeof(AudioLedPacket)) {
    badPackets = badPackets + 1;
    return;
  }

  if (p.senderChannel >= FIRST_WIFI_CHANNEL && p.senderChannel <= LAST_WIFI_CHANNEL) {
    announcedChannel = p.senderChannel;
  }

  if (AUTO_CHANNEL_SCAN && !wifiChannelLocked) {
    uint8_t ch = currentWifiChannel;
    if (ch <= LAST_WIFI_CHANNEL && channelGoodCount[ch] < 0xFFFF) {
      channelGoodCount[ch] = channelGoodCount[ch] + 1;
      channelRssiSum[ch] = channelRssiSum[ch] + rssi;
    }
  }

  lastSenderRssi = (int8_t)(rssi < -127 ? -127 : (rssi > 0 ? 0 : rssi));
  if (haveRxSequence) {
    const uint16_t missing = (uint16_t)(p.sequence - (uint16_t)(lastRxSequence + 1));
    if (missing > 0 && missing < 500) radioLost = radioLost + missing;  // a bigger jump is an S3 restart
  }
  lastRxSequence = p.sequence;
  haveRxSequence = true;
  portENTER_CRITICAL(&packetMux);
  latestPacket = p;
  memcpy(latestMac, mac, 6);
  havePacket = true;
  goodPackets = goodPackets + 1;
  packetGeneration = packetGeneration + 1;
  portEXIT_CRITICAL(&packetMux);
}

#if ESP_IDF_VERSION_MAJOR >= 5
void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  copy_packet_from_callback(info->src_addr, data, len, info->rx_ctrl ? info->rx_ctrl->rssi : -127);
}
#else
void on_data_recv(const uint8_t *mac, const uint8_t *data, int len) {
  copy_packet_from_callback(mac, data, len, -127);
}
#endif

static void set_receiver_channel(uint8_t channel) {
  if (channel < FIRST_WIFI_CHANNEL || channel > LAST_WIFI_CHANNEL) return;
  esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    currentWifiChannel = channel;
  } else {
    Serial.print("Failed to set Wi-Fi channel ");
    Serial.print(channel);
    Serial.print(" err=");
    Serial.println((uint32_t)err);
  }
}

void setup_wifi_espnow() {
  WiFi.useStaticBuffers(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  set_receiver_channel(FIRST_WIFI_CHANNEL);

  Serial.print("LED receiver MAC: ");
  Serial.println(WiFi.macAddress());
  if (AUTO_CHANNEL_SCAN) {
    Serial.print("Scanning ESP-NOW channels ");
    Serial.print(FIRST_WIFI_CHANNEL);
    Serial.print(" through ");
    Serial.println(LAST_WIFI_CHANNEL);
  } else {
    Serial.print("Listening on fixed Wi-Fi channel: ");
    Serial.println((uint8_t)currentWifiChannel);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  if (esp_now_register_recv_cb(on_data_recv) != ESP_OK) {
    Serial.println("ESP-NOW receive callback registration failed");
    while (true) delay(1000);
  }

  // Phase 16: heartbeats go back to the S3 as broadcasts on the current channel.
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, HEARTBEAT_BROADCAST, 6);
  peer.channel = 0;  // whatever channel the radio is on
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) Serial.printf("Heartbeat peer add failed: %d\n", (int)err);
}

void send_heartbeat(const HeartbeatPacket &h) {
  esp_now_send(HEARTBEAT_BROADCAST, (const uint8_t *)&h, sizeof(h));
}

static void reset_channel_survey() {
  for (uint8_t c = 0; c <= LAST_WIFI_CHANNEL; c++) {
    channelGoodCount[c] = 0;
    channelRssiSum[c] = 0;
  }
}

void maintain_channel_discovery(uint32_t now) {
  if (!AUTO_CHANNEL_SCAN) return;

  // Sender-announced channel wins over the survey: lock (or retune) straight to it.
  uint8_t announced = announcedChannel;
  if (announced != 0 && (!wifiChannelLocked || lockedWifiChannel != announced)) {
    Serial.printf("Sender announced Wi-Fi channel %u%s\n", (unsigned int)announced, wifiChannelLocked ? "; retuning" : "");
    channelSurveying = false;
    reset_channel_survey();
    set_receiver_channel(announced);
    lockedWifiChannel = announced;
    wifiChannelLocked = true;
    return;
  }

  if (wifiChannelLocked) {
    if (lockedWifiChannel != 0 && currentWifiChannel != lockedWifiChannel) {
      set_receiver_channel(lockedWifiChannel);
    }

    if (lastPacketMs > 0 && now - lastPacketMs > CHANNEL_RESCAN_TIMEOUT_MS) {
      Serial.println("Lumina packets timed out; resuming channel scan.");
      wifiChannelLocked = false;
      lockedWifiChannel = 0;
      lastChannelHopMs = 0;
      channelSurveying = false;
      reset_channel_survey();
      announcedChannel = 0;  // don't snap back to a channel that went quiet
    }
    return;
  }

  if (now - lastChannelHopMs < CHANNEL_DWELL_MS) return;
  lastChannelHopMs = now;

  if (!channelSurveying) {
    for (uint8_t c = FIRST_WIFI_CHANNEL; c <= LAST_WIFI_CHANNEL; c++) {
      if (channelGoodCount[c] > 0) {
        channelSurveying = true;
        surveyHopsLeft = LAST_WIFI_CHANNEL - FIRST_WIFI_CHANNEL + 1;
        Serial.printf("Lumina packets seen on channel %u; surveying all channels before locking.\n", (unsigned int)c);
        reset_channel_survey();  // every channel then gets exactly one dwell
        break;
      }
    }
  } else if (surveyHopsLeft > 0) {
    surveyHopsLeft--;
  }

  if (channelSurveying && surveyHopsLeft == 0) {
    uint8_t best = 0;
    uint8_t mostPackets = FIRST_WIFI_CHANNEL;
    int32_t bestRssi = INT32_MIN;
    Serial.print("Channel survey packets/avgRSSI:");
    for (uint8_t c = FIRST_WIFI_CHANNEL; c <= LAST_WIFI_CHANNEL; c++) {
      uint16_t n = channelGoodCount[c];
      int32_t avg = n ? channelRssiSum[c] / (int32_t)n : -127;
      Serial.printf(" %u:%u/%ld", (unsigned int)c, (unsigned int)n, (long)avg);
      if (n > channelGoodCount[mostPackets]) mostPackets = c;
      if (n >= SURVEY_MIN_PACKETS && avg > bestRssi) {
        bestRssi = avg;
        best = c;
      }
    }
    Serial.println();
    if (best == 0) best = mostPackets;
    reset_channel_survey();
    channelSurveying = false;
    lockedWifiChannel = best;
    wifiChannelLocked = true;
    set_receiver_channel(best);
    return;
  }

  uint8_t next = (uint8_t)currentWifiChannel + 1;
  if (next > LAST_WIFI_CHANNEL) next = FIRST_WIFI_CHANNEL;
  set_receiver_channel(next);
}
