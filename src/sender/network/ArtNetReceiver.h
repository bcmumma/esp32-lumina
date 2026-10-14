// Art-Net receive on the S3 (Phase 10: parse and report only; nothing drives the LEDs yet).
// Answers ArtPoll with this board's own LAN IP, because SoundSwitch unicasts ArtDmx only to
// nodes that answer, at the advertised address (docs/ARCHITECTURE.md, SoundSwitch baseline).
#pragma once

#include <Arduino.h>

// SoundSwitch "Universe One" arrives as Art-Net universe 0 (measured 2026-10-14).
static constexpr uint16_t ARTNET_LISTEN_UNIVERSE = 0;

void setup_artnet();
void artnet_service(uint32_t nowMs);
// Appends the /api/artnet JSON: stats, the first `channels` of the listen universe,
// and recent frames (sequence, ms, CH1-12) for matching against a PC capture.
void append_artnet_json(String &s, int channels);
