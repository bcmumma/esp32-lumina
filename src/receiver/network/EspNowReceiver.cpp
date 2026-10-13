#include "EspNowReceiver.h"
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

static void reset_channel_survey();

static void copy_packet_from_callback(const uint8_t *mac, const uint8_t *data, int len, int rssi) {
  if (len != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  AudioLedPacket p = {};
  memcpy(&p, data, sizeof(p));

  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION || p.size != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  if (AUTO_CHANNEL_SCAN && !wifiChannelLocked) {
    uint8_t ch = currentWifiChannel;
    if (ch <= LAST_WIFI_CHANNEL && channelGoodCount[ch] < 0xFFFF) {
      channelGoodCount[ch] = channelGoodCount[ch] + 1;
      channelRssiSum[ch] = channelRssiSum[ch] + rssi;
    }
  }

  portENTER_CRITICAL(&packetMux);
  latestPacket = p;
  memcpy(latestMac, mac, 6);
  havePacket = true;
  goodPackets++;
  packetGeneration++;
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
}

static void reset_channel_survey() {
  for (uint8_t c = 0; c <= LAST_WIFI_CHANNEL; c++) {
    channelGoodCount[c] = 0;
    channelRssiSum[c] = 0;
  }
}

void maintain_channel_discovery(uint32_t now) {
  if (!AUTO_CHANNEL_SCAN) return;

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
