// ESP-NOW receive path: packet hand-off to loop(), channel scan + survey lock, Wi-Fi radio setup.
#pragma once

#include <Arduino.h>
#include "LuminaProtocol.h"
#include "CellFrame.h"

// In home-Wi-Fi mode, the sender must use the router's 2.4 GHz channel.
// This receiver automatically scans channels until it finds valid Lumina
// packets, locks to that channel, and resumes scanning if packets disappear.
static constexpr bool AUTO_CHANNEL_SCAN = true;
static constexpr uint8_t FIRST_WIFI_CHANNEL = 1;
static constexpr uint8_t LAST_WIFI_CHANNEL = 11; // US 2.4 GHz channels
static constexpr uint32_t CHANNEL_DWELL_MS = 220;
static constexpr uint32_t CHANNEL_RESCAN_TIMEOUT_MS = 3000;
static constexpr uint16_t SURVEY_MIN_PACKETS = 2;

extern volatile uint8_t currentWifiChannel;
extern volatile uint8_t lockedWifiChannel;
extern volatile bool wifiChannelLocked;
extern uint32_t lastChannelHopMs;
extern uint8_t lastPrintedLockedChannel;
extern volatile uint16_t channelGoodCount[LAST_WIFI_CHANNEL + 1];
extern volatile int32_t channelRssiSum[LAST_WIFI_CHANNEL + 1];
extern bool channelSurveying;
extern uint8_t surveyHopsLeft;

// Latest packet from the ESP-NOW callback; copy it out under packetMux.
extern portMUX_TYPE packetMux;
extern AudioLedPacket latestPacket;
extern uint8_t latestMac[6];
extern volatile bool havePacket;
extern volatile uint32_t goodPackets;
extern volatile uint32_t badPackets;
extern volatile uint32_t packetGeneration;
extern uint32_t lastPacketMs;  // set by loop() on each new packet; drives the rescan timeout

// Latest SoundSwitch cell frame (Phase 15); copy it out under packetMux.
extern cells::Frame cellFrame;
extern volatile uint32_t lastCellMs;  // millis() of the last accepted cell packet, 0 = none

void setup_wifi_espnow();
// Phase 16: tell the S3 this room is alive (broadcast on the locked channel).
void send_heartbeat(const HeartbeatPacket &h);
extern volatile int8_t lastSenderRssi;
void maintain_channel_discovery(uint32_t now);
