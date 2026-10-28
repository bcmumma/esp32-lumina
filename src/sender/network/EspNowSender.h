// ESP-NOW broadcast of the Lumina packet, send callback, and send statistics.
#pragma once

#include <Arduino.h>
#include "LuminaProtocol.h"

extern uint16_t sequence_num;
extern volatile bool forceEspNowSend;
extern volatile bool espnow_busy;
extern volatile uint32_t send_cb_ok;
extern volatile uint32_t send_cb_fail;
extern uint32_t send_enqueue_ok;
extern uint32_t send_enqueue_err;
extern uint32_t send_skipped_busy;
extern uint32_t last_send_err;
extern uint32_t last_send_ms;
extern uint32_t busy_since_ms;
extern uint32_t last_pps_ms;
extern uint32_t last_pps_count;
extern uint32_t packets_per_sec;

void setup_espnow();
void maybe_send_espnow(AudioLedPacket &p);
// Phase 15: cell colours to send right after the next state packet (total = 0 sends none).
void queue_cell_frame(const uint8_t *rgb, uint8_t total);
// Phase 16: JSON array of the two rooms' status from their heartbeats (/api/control "rooms").
void append_rooms_json(String &s);
// Phase 16: ask a room receiver (1 or 2) to join Wi-Fi and accept a firmware upload.
void request_room_update(uint8_t room);
extern uint32_t cell_packets_sent;
void update_packet_rate(uint32_t now);
