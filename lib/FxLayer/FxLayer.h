// Lumina FX layer (Phase 15): turns SoundSwitch's 5-channel "Lumina FX" fixture into
// effect cues. AutoScript animates that fixture like a moving head (owner probes, 2026-10-23/25):
//   Intensity  held full in movement sections, pulsing fast in busy sections
//   Strobe     occasional (gated by the rig strobe rules downstream)
//   Pan, Tilt  sweep in movement sections (Tilt about 99..156, Pan about 113..142)
//   UV         short accent bursts
// Pure logic, no Arduino.
#pragma once

#include <stdint.h>

namespace fx {

constexpr uint8_t kChannels = 5;              // Intensity, Strobe, Pan, Tilt, UV
constexpr uint32_t kMoveHoldMs = 1500;         // beam stays on this long after Pan/Tilt last moved
constexpr uint32_t kPulseWindowMs = 1000;      // "pulsing" = 2+ big Intensity jumps within this window
constexpr uint8_t kBigJump = 64;
constexpr uint32_t kSparkleMs = 700;           // UV burst sparkle fades over this

struct Input {
  uint8_t intensity, strobe, pan, tilt, uv;
};

struct Settings {
  uint8_t beamStrength;  // 0..255
  uint8_t beamSpread;    // 1..8: how far a Pan/Tilt sweep moves the beam along the strip
};

struct State {
  bool primed;
  Input last;
  uint32_t lastMoveMs;   // 0 = never moved
  uint32_t jumpMs[2];    // times of the last two big Intensity jumps
  uint32_t uvMs;         // last UV burst onset, 0 = none
  uint32_t edges;        // Intensity rising edges through 128 (backup hit source)
};

struct Output {
  uint8_t beamPos;    // 0..255 along the strip
  uint8_t beamLevel;  // 0 = no beam
  bool moving;
  bool pulsing;       // busy section: kick effects on
  uint8_t sparkle;    // 0..255, fades after a UV burst
  uint8_t strobe;     // FX Strobe channel (0-9 steady)
  bool hit;           // this frame had an Intensity rising edge
};

void reset(State &s);

// Reads the 5 channels at the 1-based DMX address. False if dmx is null or the address is 0.
bool read(const uint8_t *dmx, uint16_t length, uint16_t address, Input &out);

// Feed every call; newFrame = true once per new Art-Net frame (edges and jumps are judged then).
Output step(State &s, const Input &in, bool newFrame, const Settings &cfg, uint32_t nowMs);

}  // namespace fx
