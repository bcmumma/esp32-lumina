// The render modes 0..LED_MODE_MAX (see the mode list in Effects.cpp). Each draws one strand for one frame.
#pragma once

#include <Arduino.h>
#include "LuminaProtocol.h"

static constexpr bool FORCE_VISIBLE_IF_BAD_SETTINGS = false; // normal operation: Off/disabled/white must be honored
static constexpr uint8_t SAFE_BRIGHTNESS = 220;

// Declared before any function definitions so the Arduino .ino preprocessor
// can generate valid prototypes for VU helper functions.
enum VuSource : uint8_t {
  VU_SOURCE_OVERALL = 0,
  VU_SOURCE_BASS = 1,
  VU_SOURCE_MID = 2,
  VU_SOURCE_TREBLE = 3
};

void render_off(uint8_t s);
void render_solid(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_debug_chase(uint8_t s, const AudioLedPacket &p);
void render_space_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_beat_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_bass_comet(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_twinkle(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_spectrum(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_band_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, VuSource source);
void render_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_beat_tracker(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_glitter_kick(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_aurora_drift(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_starfield(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_neon_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_laser_sweep(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_rave_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_bass_cannons(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_dnb_chopper(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_acid_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_portal_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_spectrum_sparks(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_hyper_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_cyber_tunnel(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_white_lightning(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_dnb_split(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_rave_tiles(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_pulse_train(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_void_bloom(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_stereo_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_kick_pump(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_kick_ripple(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_break_chopper(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
void render_acid_squelch(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st);
void render_drop_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat);
// Kick Flash overlay: update once per frame, then apply after each strand's mode.
void update_kick_flash(const AudioLedPacket &p, bool newBeat);
void apply_kick_flash(uint8_t s, const AudioLedPacket &p);

// Rig-wide strobe gate; .events (flash events since boot) is printed in the status line.
extern StrobeGate strobeGate;
