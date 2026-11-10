#include "Effects.h"
#include "Color.h"
#include "EffectState.h"
#include "LedOutput.h"
#include "Mood.h"

StrandRuntime rt[MAX_STRANDS];
StrobeGate strobeGate = {0, 0};  // one flash timer for the whole rig (all strands)

// Strobe safety (docs/DECISIONS.md): every flash onset goes through here, so the
// sender's global strobe switch and max flash rate (packet flags) always apply.
static bool flash_allowed(uint8_t s, const AudioLedPacket &p, uint32_t now) {
  if (!strobe_gate_allow(strobeGate, p.flags, now)) return false;
  rt[s].lastTriggerMs = now;
  return true;
}

// Whole-strip flashes (Strobe, Drop Strobe, Beat Strobe, Kick Flash) also need the Strobe button
// (packet.fullStrobe, after strobe sensitivity). Owner, 2026-11-09: with Strobe off nothing flashes
// the whole strip; the flash looks keep only their own small flashes.
static bool full_flash_allowed(uint8_t s, const AudioLedPacket &p, uint32_t now) {
  return p.fullStrobe && flash_allowed(s, p, now);
}

static uint8_t combined_brightness(const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t gb = p.globalBrightness;
  uint8_t sb = st.brightness;
  if (FORCE_VISIBLE_IF_BAD_SETTINGS) {
    if (gb == 0) gb = SAFE_BRIGHTNESS;
    if (sb == 0) sb = SAFE_BRIGHTNESS;
  }
  return scale8_video(sb, gb);
}

// A look colour: the strand hue plus the effect's offset. Under a colour mood it is folded into the
// mood's hue range and drifts slowly between its two ends (lib/Mood). Halloween looks use fixed
// colours (hsv_to_rgb directly), so moods leave them alone.
static Rgb strand_color(const AudioLedPacket &p, const StrandSettings &st, uint8_t offset = 0, uint8_t value = 255) {
  const uint8_t hue = (uint8_t)(st.hue + p.globalHue + offset);
  if (!mood::active(st)) return hsv_to_rgb(hue, st.saturation, value);
  return hsv_to_rgb(mood::fold(st, hue, mood::drift(p.ms)), max<uint8_t>(st.saturation, mood::kMinSaturation), value);
}

// Strobe-type flashes (Kick Flash, the strobe modes, Hyper bursts, the gated kick looks) use the
// strand's strobe colour: white unless the DJ picked one. Moods never recolour it.
static Rgb strobe_colour(const StrandSettings &st, uint8_t value) {
  if (strobe_color_is_white(st.strobeColor)) return {value, value, value};
  return hsv_to_rgb(strobe_color_hue(st.strobeColor), 255, value);
}

// Strobe brightness (packet v13, the Live page slider) scales every strobe-type flash.
static uint8_t strobeLevelNow = 255;
void effects_begin_frame(const AudioLedPacket &p) { strobeLevelNow = p.strobeLevel; }

static Rgb strobe_rgb(const StrandSettings &st, uint8_t value) {
  return strobe_colour(st, scale8_video(value, strobeLevelNow));
}

// Phase 15: effects were tuned on 150-LED strands. Counts and speeds that are "per strand"
// scale by length so a 900-LED strand looks the same; 150 or fewer are unchanged.
static constexpr uint16_t TUNED_STRAND_LEDS = 150;
static uint16_t len_scale(uint16_t n, uint16_t v) {
  return n <= TUNED_STRAND_LEDS ? v : (uint16_t)(((uint32_t)v * n + TUNED_STRAND_LEDS / 2) / TUNED_STRAND_LEDS);
}

static uint8_t scaled_audio(uint8_t v, uint8_t sensitivity) {
  uint16_t x = ((uint16_t)v * (uint16_t)(sensitivity + 32)) / 160;
  return x > 255 ? 255 : (uint8_t)x;
}

// ============================================================
// Render modes
// Public v22 list:
//   9 Solid Color
//   15 Aurora Drift, 16 Starfield, 17 Neon Scanner
//   18 Laser Sweep, 19 Rave Chase, 20 Bass Cannons
//   21 DnB Chopper, 22 Acid Chase, 23 Portal Pulse
//   24 Spectrum Sparks, 25 Hyper Strobe, 26 Cyber Tunnel
//   27 White Lightning, 28 DnB Split, 29 Rave Tiles
//   30 Pulse Train, 31 Void Bloom
//   Phase 14: 32 Stereo VU, 33 Kick Pump, 34 Kick Ripple, 35 Break Chopper, 36 Acid Squelch,
//   37 Strobe, 38 Drop Strobe; plus the rig-wide Kick Flash overlay
//   39 SoundSwitch cells (Phase 15, set by the S3, not listed on the page)
//   40 Candle Flicker, 41 Witch's Brew, 42 Ghost Drift, 43 Pumpkin Patch, 44 Blood Drip, 45 Graveyard Fog (Halloween)
//   46 Hyper Scatter, 47 Hyper Grid, 48 Hyper Mirror, 49 Split Mirror, 50 Split Blocks, 51 Split Strobe (Club)
//   52 DnB Split Original (28 without colour moods or strobe colour)
// The split looks (28, 49, 51, 52) read the bands through the packet's EQ (eq_apply, 128 = flat).
// Every flash onset goes through flash_allowed: flags bit 7 (Setup "Flashing looks") and the max rate.
// Whole-strip flashes also need the Strobe button (full_flash_allowed); it flashes the strip on kicks.
// Internal/legacy modes 0-14 remain available for VU tuning and compatibility.
// ============================================================
void render_off(uint8_t s) {
  clear_strand(s);
}

void render_solid(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t b = combined_brightness(p, st);
  fill_strand(s, strand_color(p, st, 0, b));
}

void render_debug_chase(uint8_t s, const AudioLedPacket &p) {
  // Debug chase uses logical blue; Color Calibration maps it to your strip.
  // If this moves, ESP-NOW packets are controlling the LEDs.
  uint16_t n = count_for(s);
  if (n == 0) return;

  clear_strand(s);
  uint16_t pos = (p.sequence / 2) % n;

  Rgb blue = {0, 0, 255};
  Rgb dimBlue = {0, 0, 32};

  for (uint16_t i = 0; i < n; i++) set_pixel(s, i, dimBlue);
  set_pixel(s, pos, blue);
  if (pos > 0) set_pixel(s, pos - 1, {0, 0, 120});
}

void render_space_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t audio = scaled_audio(p.volume, st.sensitivity);
  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(30, audio));
  fill_strand(s, strand_color(p, st, 0, b));
}

void render_beat_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  if (newBeat && full_flash_allowed(s, p, millis())) rt[s].flash = 255;
  fade_strand(s, st.strobe > 0 ? st.strobe : 80);
  if (rt[s].flash > 10) {
    fill_strand(s, strobe_rgb(st, combined_brightness(p, st)));
    rt[s].flash = scale8(rt[s].flash, 150);
  }
}

void render_bass_comet(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, 55);

  uint8_t step = max<uint8_t>(1, st.speed / 45);
  rt[s].pos = (rt[s].pos + step) % n;

  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(60, scaled_audio(p.bass, st.sensitivity)));
  Rgb c = strand_color(p, st, 0, b);

  for (uint8_t t = 0; t < 6; t++) {
    int idx = (int)rt[s].pos - t;
    if (idx < 0) idx += n;
    add_pixel(s, (uint16_t)idx, scale_color(c, 255 - t * 35));
  }
}

void render_twinkle(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  fade_strand(s, 25);
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint8_t chance = map(st.twinkle, 0, 255, 1, 18);
  if (newBeat) chance = min<uint8_t>(40, chance + 18);
  const uint16_t spawns = len_scale(n, chance);

  for (uint16_t k = 0; k < spawns; k++) {
    uint16_t idx = random(n);
    uint8_t v = random(80, 255);
    add_pixel(s, idx, strand_color(p, st, random(0, 80), scale8_video(v, combined_brightness(p, st))));
  }
}

void render_spectrum(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint16_t third = max<uint16_t>(1, n / 3);
  uint8_t bassB = scale8_video(combined_brightness(p, st), scaled_audio(p.bass, st.sensitivity));
  uint8_t midB = scale8_video(combined_brightness(p, st), scaled_audio(p.mid, st.sensitivity));
  uint8_t trebleB = scale8_video(combined_brightness(p, st), scaled_audio(p.treble, st.sensitivity));

  for (uint16_t i = 0; i < n; i++) {
    if (i < third) set_pixel(s, i, strand_color(p, st, 0, bassB));
    else if (i < third * 2) set_pixel(s, i, strand_color(p, st, 70, midB));
    else set_pixel(s, i, strand_color(p, st, 145, trebleB));
  }
}

static uint8_t vu_source_value(const AudioLedPacket &p, VuSource source) {
  switch (source) {
    case VU_SOURCE_BASS: return p.bass;
    case VU_SOURCE_MID: return p.mid;
    case VU_SOURCE_TREBLE: return p.treble;
    case VU_SOURCE_OVERALL:
    default: return p.volume;
  }
}

static Rgb vu_color(VuSource source, uint8_t position255, uint8_t value) {
  switch (source) {
    case VU_SOURCE_BASS: {
      // Deep red through amber.
      uint8_t hue = (uint8_t)(4 + ((uint16_t)position255 * 24U) / 255U);
      return hsv_to_rgb(hue, 255, value);
    }
    case VU_SOURCE_MID: {
      // Green through cyan.
      uint8_t hue = (uint8_t)(88 + ((uint16_t)position255 * 42U) / 255U);
      return hsv_to_rgb(hue, 255, value);
    }
    case VU_SOURCE_TREBLE: {
      // Blue through purple.
      uint8_t hue = (uint8_t)(160 + ((uint16_t)position255 * 42U) / 255U);
      return hsv_to_rgb(hue, 255, value);
    }
    case VU_SOURCE_OVERALL:
    default: {
      // Classic green -> yellow -> red level meter.
      uint8_t hue;
      if (position255 < 170) {
        hue = (uint8_t)(96 - ((uint16_t)position255 * 54U) / 170U);
      } else {
        hue = (uint8_t)(42 - ((uint16_t)(position255 - 170U) * 42U) / 85U);
      }
      return hsv_to_rgb(hue, 255, value);
    }
  }
}

static void set_meter_pixel(uint8_t s, uint16_t logicalIndex, uint16_t n, uint8_t direction, Rgb color) {
  if (n == 0 || logicalIndex >= n) return;
  uint16_t physical = logicalIndex;
  if (direction == 1) physical = (n - 1U) - logicalIndex;
  set_pixel(s, physical, color);
}

void render_band_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, VuSource source) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);

  uint8_t audio = scaled_audio(vu_source_value(p, source), st.sensitivity);
  uint8_t brightness = combined_brightness(p, st);

  if (audio >= rt[s].meterPeak) {
    rt[s].meterPeak = audio;
  } else {
    // Higher Strobe/decay setting gives a slower peak-dot fall.
    uint8_t decay = (uint8_t)(1U + ((uint16_t)(255U - st.strobe) * 6U) / 255U);
    rt[s].meterPeak = rt[s].meterPeak > decay ? (uint8_t)(rt[s].meterPeak - decay) : 0;
  }

  if (st.direction == 2) {
    // Center-out meter. Each half represents the complete 0-255 range.
    uint16_t half = (n + 1U) / 2U;
    uint16_t litHalf = ((uint32_t)audio * half + 254U) / 255U;
    for (uint16_t i = 0; i < litHalf && i < half; i++) {
      uint8_t pos255 = (uint8_t)(((uint32_t)i * 255U) / max<uint16_t>(1, half - 1U));
      Rgb c = vu_color(source, pos255, brightness);
      int left = (int)((n - 1U) / 2U) - (int)i;
      int right = (int)(n / 2U) + (int)i;
      if (left >= 0) set_pixel(s, (uint16_t)left, c);
      if (right < n && right != left) set_pixel(s, (uint16_t)right, c);
    }
  } else {
    uint16_t lit = ((uint32_t)audio * n + 254U) / 255U;
    for (uint16_t i = 0; i < lit && i < n; i++) {
      uint8_t pos255 = (uint8_t)(((uint32_t)i * 255U) / max<uint16_t>(1, n - 1U));
      set_meter_pixel(s, i, n, st.direction, vu_color(source, pos255, brightness));
    }
  }

  // White peak marker makes gain/AGC tuning easier to read at a glance.
  if (rt[s].meterPeak > 0) {
    uint16_t peakIndex = ((uint32_t)rt[s].meterPeak * (n - 1U)) / 255U;
    if (st.direction == 2) {
      uint16_t half = (n + 1U) / 2U;
      uint16_t peakHalf = ((uint32_t)rt[s].meterPeak * max<uint16_t>(1, half - 1U)) / 255U;
      int left = (int)((n - 1U) / 2U) - (int)peakHalf;
      int right = (int)(n / 2U) + (int)peakHalf;
      Rgb marker = {brightness, brightness, brightness};
      if (left >= 0) set_pixel(s, (uint16_t)left, marker);
      if (right < n && right != left) set_pixel(s, (uint16_t)right, marker);
    } else {
      set_meter_pixel(s, peakIndex, n, st.direction, {brightness, brightness, brightness});
    }
  }
}

void render_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  render_band_vu(s, p, st, VU_SOURCE_OVERALL);
}

// Phase 14: left level fills from the centre toward pixel 0, right level toward the far end.
// Direction 1 (reverse) swaps the sides for strips mounted the other way round.
void render_stereo_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t brightness = combined_brightness(p, st);
  uint8_t lo = scaled_audio(st.direction == 1 ? p.right : p.left, st.sensitivity);
  uint8_t hi = scaled_audio(st.direction == 1 ? p.left : p.right, st.sensitivity);
  uint16_t half = (n + 1U) / 2U;
  uint16_t litLo = ((uint32_t)lo * half + 254U) / 255U;
  uint16_t litHi = ((uint32_t)hi * half + 254U) / 255U;
  for (uint16_t i = 0; i < half; i++) {
    Rgb c = vu_color(VU_SOURCE_OVERALL, (uint8_t)(((uint32_t)i * 255U) / max<uint16_t>(1, half - 1U)), brightness);
    int a = (int)((n - 1U) / 2U) - (int)i;
    int b = (int)(n / 2U) + (int)i;
    if (i < litLo && a >= 0) set_pixel(s, (uint16_t)a, c);
    if (i < litHi && b < n && b != a) set_pixel(s, (uint16_t)b, c);
  }
}

void render_beat_tracker(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);

  uint8_t brightness = combined_brightness(p, st);
  uint8_t confidence = scaled_audio(p.beatConfidence, st.sensitivity);
  uint16_t confidenceLit = ((uint32_t)confidence * n + 254U) / 255U;

  // Purple confidence bar. Threshold-level detections normally occupy roughly
  // two thirds of the strip; very strong onsets reach the end.
  for (uint16_t i = 0; i < confidenceLit && i < n; i++) {
    uint8_t pos255 = (uint8_t)(((uint32_t)i * 255U) / max<uint16_t>(1, n - 1U));
    uint8_t hue = (uint8_t)(175U + ((uint16_t)pos255 * 30U) / 255U);
    set_pixel(s, i, hsv_to_rgb(hue, 255, scale8_video(brightness, 150)));
  }

  // Moving cyan phase marker shows the predicted position within the learned
  // beat period. It remains at pixel zero until tempo is established.
  uint16_t phasePos = ((uint32_t)p.beatPhase * max<uint16_t>(1, n - 1U)) / 255U;
  set_pixel(s, phasePos, {0, brightness, brightness});

  // Candidate and tempo-assist indicators at the ends of the strip.
  if (p.flags & 0x01) set_pixel(s, 0, {brightness, brightness, 0});
  if (p.flags & 0x02) set_pixel(s, n - 1U, {0, brightness, brightness});

  if (newBeat && flash_allowed(s, p, millis())) rt[s].flash = 255;
  if (rt[s].flash > 2) {
    uint8_t flashBrightness = scale8_video(brightness, rt[s].flash);
    Rgb flashColor = strand_color(p, st, 0, flashBrightness);
    for (uint16_t i = 0; i < n; i++) add_pixel(s, i, flashColor);

    // Higher Strobe/decay means a longer visible beat flash.
    uint8_t keep = (uint8_t)(120U + ((uint16_t)st.strobe * 115U) / 255U);
    rt[s].flash = scale8(rt[s].flash, keep);
  }
}

void render_glitter_kick(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  fade_strand(s, 40);
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint8_t sparkles = st.density / 28;
  if (newBeat) sparkles += 16;
  const uint16_t spawns = len_scale(n, sparkles);

  for (uint16_t k = 0; k < spawns; k++) {
    add_pixel(s, random(n), strand_color(p, st, random(0, 180), combined_brightness(p, st)));
  }
}

void render_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, 35);

  uint8_t step = max<uint8_t>(1, st.speed / 50);
  rt[s].pos = (rt[s].pos + step) % (n * 2);
  int pos = rt[s].pos;
  if (pos >= n) pos = (n * 2 - 1) - pos;

  add_pixel(s, pos, strand_color(p, st, 0, combined_brightness(p, st)));
}


static uint8_t triwave8(uint8_t x) {
  return (x & 0x80U) ? (uint8_t)(255U - ((x & 0x7FU) << 1U)) : (uint8_t)((x & 0x7FU) << 1U);
}

static uint16_t wrap_index(int32_t value, uint16_t n) {
  if (n == 0) return 0;
  int32_t m = value % (int32_t)n;
  if (m < 0) m += n;
  return (uint16_t)m;
}

static uint16_t oriented_index(uint16_t logical, uint16_t n, uint8_t direction) {
  if (n == 0) return 0;
  logical %= n;
  if (direction == 1) return (n - 1U) - logical;
  return logical;
}

static uint8_t energy_mix(const AudioLedPacket &p) {
  uint16_t e = (uint16_t)p.volume * 2U + p.bass + p.mid + p.treble;
  return (uint8_t)min<uint16_t>(255U, e / 5U);
}

static uint8_t audio_gate(uint8_t value, uint8_t sensitivity, uint8_t floorValue = 18) {
  uint8_t scaled = scaled_audio(value, sensitivity);
  if (scaled <= floorValue) return 0;
  return (uint8_t)map(scaled, floorValue, 255, 0, 255);
}

void render_aurora_drift(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  uint32_t t = millis();
  uint8_t base = combined_brightness(p, st);
  uint8_t threshold = (uint8_t)map(st.density, 0, 255, 205, 75);
  uint16_t timeA = (uint16_t)(t / max<uint16_t>(10, 74U - st.speed / 4U));
  uint16_t timeB = (uint16_t)(t / max<uint16_t>(13, 112U - st.speed / 5U));

  for (uint16_t i = 0; i < n; i++) {
    uint8_t a = triwave8((uint8_t)(i * 9U + timeA));
    uint8_t b = triwave8((uint8_t)(i * 5U - timeB));
    uint8_t cloud = (uint8_t)(((uint16_t)a * 3U + (uint16_t)b * 2U) / 5U);
    if (cloud < threshold) {
      set_pixel(s, i, {0,0,0});
      continue;
    }
    uint8_t normalized = (uint8_t)map(cloud, threshold, 255, 0, base);
    uint8_t hueOffset = (uint8_t)(32U + (i * 58U) / max<uint16_t>(1, n));
    set_pixel(s, i, strand_color(p, st, hueOffset, normalized));
  }
}

void render_starfield(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  uint8_t fadeAmount = (uint8_t)map(st.strobe, 0, 255, 52, 10);
  fade_strand(s, fadeAmount);

  uint8_t spawn = (uint8_t)map(st.density, 0, 255, 1, max<uint16_t>(2, min<uint16_t>(24, n / 3 + 1)));
  if ((uint8_t)random(0, 255) < max<uint8_t>(8, st.speed / 2)) {
    for (uint8_t k = 0; k < spawn; k++) {
      uint16_t idx = (uint16_t)random(n);
      uint8_t v = scale8_video(combined_brightness(p, st), (uint8_t)random(130, 256));
      if ((uint8_t)random(0, 100) < 32) add_pixel(s, idx, {v,v,v});
      else add_pixel(s, idx, strand_color(p, st, (uint8_t)random(0, 80), v));
    }
  }
}

void render_neon_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 75, 22));
  uint32_t t = millis();
  uint32_t travel = (t * (18U + st.speed)) / 900U;
  uint16_t cycle = n > 1 ? (uint16_t)(2U * n - 2U) : 1U;
  uint16_t q = cycle ? (uint16_t)(travel % cycle) : 0;
  uint16_t pos = q < n ? q : (cycle - q);
  if (st.direction == 1) pos = n - 1U - pos;
  uint8_t b = combined_brightness(p, st);
  uint8_t tail = (uint8_t)map(st.density, 0, 255, 2, min<uint16_t>(18, max<uint16_t>(3, n / 3)));
  for (uint8_t k = 0; k < tail; k++) {
    int32_t idx = (int32_t)pos - (st.direction == 1 ? -(int32_t)k : (int32_t)k);
    if (idx >= 0 && idx < n) add_pixel(s, (uint16_t)idx, strand_color(p, st, k * 4U, scale8_video(b, 255U - (uint16_t)k * 220U / tail)));
  }
}

void render_laser_sweep(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint32_t t = millis();
  uint8_t beams = (uint8_t)map(st.density, 0, 255, 2, 6);
  uint8_t b = combined_brightness(p, st);
  uint16_t cycle = max<uint16_t>(1, n * 2U);
  for (uint8_t beam = 0; beam < beams; beam++) {
    uint32_t travel = (t * (20U + st.speed + beam * 7U)) / (730U + beam * 83U);
    uint16_t q = (uint16_t)((travel + ((uint32_t)beam * cycle / beams)) % cycle);
    uint16_t pos = q < n ? q : (cycle - 1U - q);
    pos = oriented_index(pos, n, st.direction);
    Rgb c = strand_color(p, st, (uint8_t)(beam * 43U), b);
    set_pixel(s, pos, c);
    if (pos + 1U < n) set_pixel(s, pos + 1U, scale_color(c, 125));
  }
}

void render_rave_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t block = (uint8_t)map(st.density, 0, 255, 1, 7);
  uint8_t gap = max<uint8_t>(2, (uint8_t)map(st.density, 0, 255, 10, 3));
  uint16_t period = block + gap;
  uint16_t shift = (uint16_t)((millis() * (20U + st.speed)) / 900U);
  uint8_t b = combined_brightness(p, st);
  for (uint16_t i = 0; i < n; i++) {
    uint16_t logical = oriented_index(i, n, st.direction);
    uint16_t cell = (logical + shift) % period;
    if (cell < block) {
      uint8_t group = (uint8_t)(((logical + shift) / period) * 47U);
      set_pixel(s, i, strand_color(p, st, group, b));
    }
  }
}

void render_bass_cannons(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 100, 28));
  uint8_t bass = audio_gate(p.bass, st.sensitivity, 20);
  if (!bass) return;
  uint16_t half = (n + 1U) / 2U;
  uint16_t length = max<uint16_t>(1, ((uint32_t)bass * half) / 255U);
  uint8_t b = scale8_video(combined_brightness(p, st), bass);
  for (uint16_t i = 0; i < length; i++) {
    uint8_t fade = (uint8_t)(255U - ((uint32_t)i * 170U) / max<uint16_t>(1, length));
    Rgb c = strand_color(p, st, (uint8_t)(i * 2U), scale8_video(b, fade));
    add_pixel(s, i, c);
    add_pixel(s, n - 1U - i, c);
  }
}

void render_dnb_chopper(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t bands[3] = {
    audio_gate(p.bass, st.sensitivity, 16),
    audio_gate(p.mid, st.sensitivity, 16),
    audio_gate(p.treble, st.sensitivity, 16)
  };
  uint8_t slice = (uint8_t)(((millis() * (35U + st.speed)) / 520U) & 0x0FU);
  uint8_t tile = max<uint8_t>(1, (uint8_t)map(st.density, 0, 255, 1, 6));
  uint8_t gap = max<uint8_t>(1, 7U - tile);
  uint8_t period = tile + gap;
  for (uint16_t i = 0; i < n; i++) {
    uint8_t group = (uint8_t)(((i / period) + slice) % 3U);
    if (((i + slice) % period) >= tile || bands[group] < 10) continue;
    uint8_t b = scale8_video(combined_brightness(p, st), bands[group]);
    set_pixel(s, oriented_index(i, n, st.direction), strand_color(p, st, (uint8_t)(group * 82U + slice * 3U), b));
  }
}

void render_acid_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 90, 30));
  uint8_t energy = max<uint8_t>(42, audio_gate((uint8_t)(((uint16_t)p.mid + p.treble) / 2U), st.sensitivity, 12));
  uint8_t heads = (uint8_t)map(st.density, 0, 255, 1, 5);
  uint8_t b = scale8_video(combined_brightness(p, st), energy);
  uint32_t shift = (millis() * (24U + st.speed + energy / 3U)) / 650U;
  for (uint8_t h = 0; h < heads; h++) {
    uint16_t pos = (uint16_t)((shift + (uint32_t)h * n / heads) % n);
    pos = oriented_index(pos, n, st.direction);
    for (uint8_t tail = 0; tail < 5; tail++) {
      uint16_t idx = wrap_index((int32_t)pos - (st.direction == 1 ? -(int32_t)tail : (int32_t)tail), n);
      add_pixel(s, idx, strand_color(p, st, (uint8_t)(h * 51U + shift / 3U), scale8_video(b, 255U - tail * 45U)));
    }
  }
}

void render_portal_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t energy = audio_gate(max<uint8_t>(p.volume, p.bass), st.sensitivity, 12);
  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(35, energy));
  uint8_t rings = (uint8_t)map(st.density, 0, 255, 2, 8);
  uint16_t phase = (uint16_t)((millis() * (12U + st.speed)) / 850U);
  int centerL = (int)((n - 1U) / 2U);
  int centerR = (int)(n / 2U);
  for (uint8_t r = 0; r < rings; r++) {
    uint16_t radius = (phase + (uint32_t)r * max<uint16_t>(2, n / rings)) % max<uint16_t>(1, n / 2U + 1U);
    Rgb c = strand_color(p, st, (uint8_t)(r * 36U), scale8_video(b, (uint8_t)(255U - r * 150U / rings)));
    if (centerL - (int)radius >= 0) set_pixel(s, (uint16_t)(centerL - radius), c);
    if (centerR + (int)radius < n) set_pixel(s, (uint16_t)(centerR + radius), c);
  }
}

void render_spectrum_sparks(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 95, 24));
  uint8_t values[3] = {
    audio_gate(p.bass, st.sensitivity, 14),
    audio_gate(p.mid, st.sensitivity, 14),
    audio_gate(p.treble, st.sensitivity, 14)
  };
  uint8_t maxSpawns = (uint8_t)map(st.density, 0, 255, 2, 18);
  for (uint8_t band = 0; band < 3; band++) {
    uint8_t spawns = (uint8_t)(((uint16_t)values[band] * maxSpawns) / 255U);
    for (uint8_t k = 0; k < spawns; k++) {
      uint16_t idx = (uint16_t)random(n);
      uint8_t b = scale8_video(combined_brightness(p, st), (uint8_t)random(max<uint8_t>(40, values[band] / 2U), max<uint16_t>(41, values[band] + 1U)));
      add_pixel(s, idx, strand_color(p, st, (uint8_t)(band * 82U + random(0, 24)), b));
    }
  }
}

void render_hyper_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  uint8_t energy = energy_mix(p);
  int16_t delta = (int16_t)energy - (int16_t)rt[s].prevEnergy;
  rt[s].prevEnergy = energy;
  uint8_t threshold = (uint8_t)map(st.strobe, 0, 255, 72, 14);
  uint32_t now = millis();
  if (delta > threshold && flash_allowed(s, p, now)) {  // was: every 35 ms (~28/s)
    rt[s].flash = (uint8_t)min<int>(255, 120 + delta * 3);
    rt[s].aux = (uint16_t)random(max<uint16_t>(1, n));
  }
  clear_strand(s);
  if (rt[s].flash < 10) return;
  uint8_t width = (uint8_t)map(st.density, 0, 255, 1, max<uint16_t>(2, min<uint16_t>(18, n)));
  uint8_t b = scale8_video(combined_brightness(p, st), rt[s].flash);
  for (uint8_t i = 0; i < width; i++) {
    set_pixel(s, wrap_index((int32_t)rt[s].aux + i, n), (i & 1U) ? strand_color(p, st, 110, b) : strobe_rgb(st, b));
  }
  rt[s].flash = scale8(rt[s].flash, (uint8_t)map(st.strobe, 0, 255, 90, 205));
}

void render_cyber_tunnel(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t energy = max<uint8_t>(48, audio_gate(p.volume, st.sensitivity, 10));
  uint8_t b = scale8_video(combined_brightness(p, st), energy);
  uint8_t dashes = (uint8_t)map(st.density, 0, 255, 3, 10);
  uint16_t half = max<uint16_t>(1, n / 2U);
  uint16_t phase = (uint16_t)((millis() * (18U + st.speed)) / 700U);
  for (uint8_t d = 0; d < dashes; d++) {
    uint16_t radius = (phase + (uint32_t)d * max<uint16_t>(1, half / dashes)) % half;
    uint8_t fade = (uint8_t)(255U - ((uint16_t)d * 170U) / dashes);
    Rgb c = strand_color(p, st, (uint8_t)(d * 31U), scale8_video(b, fade));
    uint16_t left = half - 1U - radius;
    uint16_t right = min<uint16_t>(n - 1U, half + radius);
    set_pixel(s, left, c);
    set_pixel(s, right, c);
  }
}

void render_white_lightning(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 120, 34));
  int16_t delta = (int16_t)p.treble - (int16_t)rt[s].prevTreble;
  rt[s].prevTreble = p.treble;
  uint8_t threshold = (uint8_t)map(st.sensitivity, 0, 255, 65, 12);
  uint32_t now = millis();
  if (delta > threshold && flash_allowed(s, p, now)) {  // was: every 45 ms (~22/s)
    rt[s].flash = (uint8_t)min<int>(255, 135 + delta * 3);
    uint8_t forks = (uint8_t)map(st.density, 0, 255, 1, 5);
    for (uint8_t f = 0; f < forks; f++) {
      int32_t start = random(n);
      uint8_t len = (uint8_t)map(st.twinkle, 0, 255, 2, max<uint16_t>(3, min<uint16_t>(22, n)));
      for (uint8_t k = 0; k < len; k++) {
        int32_t idx = start + (int32_t)k * (random(0, 2) ? 1 : -1);
        if (idx < 0 || idx >= n) continue;
        uint8_t b = scale8_video(combined_brightness(p, st), (uint8_t)(255U - (uint16_t)k * 180U / len));
        add_pixel(s, (uint16_t)idx, (k & 1U) ? Rgb{0,(uint8_t)(b / 2U),b} : strobe_rgb(st, b));
      }
    }
  }
}

void render_dnb_split(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t values[3] = {eq_apply(p.bass, p.eq[0]), eq_apply(p.mid, p.eq[1]), eq_apply(p.treble, p.eq[2])};  // split-look EQ
  uint16_t start = 0;
  for (uint8_t band = 0; band < 3; band++) {
    uint16_t end = (uint16_t)(((uint32_t)(band + 1U) * n) / 3U);
    uint16_t width = max<uint16_t>(1, end - start);
    uint8_t level = audio_gate(values[band], st.sensitivity, 12);
    uint16_t lit = ((uint32_t)level * width) / 255U;
    uint8_t tile = max<uint8_t>(1, (uint8_t)map(st.density, 0, 255, 1, 5));
    for (uint16_t i = 0; i < lit; i++) {
      if (((i + (millis() * (10U + st.speed) / 700U)) / tile) & 1U) continue;
      uint8_t b = scale8_video(combined_brightness(p, st), level);
      set_pixel(s, oriented_index(start + i, n, st.direction), strand_color(p, st, band * 82U, b));
    }
    start = end;
  }
}

void render_rave_tiles(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t tile = max<uint8_t>(1, (uint8_t)map(st.density, 0, 255, 1, 8));
  uint16_t shift = (uint16_t)((millis() * (16U + st.speed)) / 720U);
  uint8_t values[3] = {p.bass, p.mid, p.treble};
  for (uint16_t i = 0; i < n; i++) {
    uint16_t group = (i + shift) / tile;
    if ((group & 1U) == 0U) continue;
    uint8_t band = (uint8_t)(group % 3U);
    uint8_t level = max<uint8_t>(35, audio_gate(values[band], st.sensitivity, 12));
    uint8_t b = scale8_video(combined_brightness(p, st), level);
    set_pixel(s, oriented_index(i, n, st.direction), strand_color(p, st, (uint8_t)(band * 82U + shift / 2U), b));
  }
}

void render_pulse_train(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 105, 34));
  uint8_t energy = max<uint8_t>(42, audio_gate(p.volume, st.sensitivity, 10));
  uint8_t pulses = (uint8_t)map(st.density, 0, 255, 2, 8);
  uint32_t travel = (millis() * (22U + st.speed + energy / 4U)) / 680U;
  uint8_t b = scale8_video(combined_brightness(p, st), energy);
  for (uint8_t q = 0; q < pulses; q++) {
    uint16_t pos = (uint16_t)((travel + (uint32_t)q * n / pulses) % n);
    pos = oriented_index(pos, n, st.direction);
    add_pixel(s, pos, strand_color(p, st, q * 37U, b));
    add_pixel(s, wrap_index((int32_t)pos - 1, n), strand_color(p, st, q * 37U, scale8_video(b, 110)));
  }
}

void render_void_bloom(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t energy = audio_gate((uint8_t)(((uint16_t)p.volume + p.bass) / 2U), st.sensitivity, 10);
  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(25, energy));
  uint8_t blooms = (uint8_t)map(st.density, 0, 255, 1, 5);
  uint16_t half = max<uint16_t>(1, n / 2U);
  uint16_t phase = (uint16_t)((millis() * (9U + st.speed)) / 980U);
  for (uint8_t bloom = 0; bloom < blooms; bloom++) {
    uint16_t radius = (phase + (uint32_t)bloom * max<uint16_t>(2, half / blooms)) % half;
    if (((phase / max<uint16_t>(1, half)) + bloom) & 1U) continue; // intentionally empty cycles
    uint8_t fade = (uint8_t)(255U - ((uint16_t)bloom * 175U) / blooms);
    Rgb c = strand_color(p, st, (uint8_t)(bloom * 49U), scale8_video(b, fade));
    int32_t center = n / 2;
    int32_t left = center - radius;
    int32_t right = center + radius;
    if (left >= 0) set_pixel(s, (uint16_t)left, c);
    if (right < n) set_pixel(s, (uint16_t)right, c);
  }
}

// ============================================================
// Phase 14: bass-hit modes. newBeat is set when the sender's bass-hit pulse
// changes (a transient, not tempo tracking). Any whole-strand flash goes
// through flash_allowed(), so the global strobe switch and rate cap apply.
// ============================================================

// Techno/house. A dim colour bed follows the bass; each hit pumps the whole strand.
// With strobe off the pump is suppressed and only the smooth bass bed remains.
void render_kick_pump(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  if (count_for(s) == 0) return;
  if (newBeat && flash_allowed(s, p, millis())) rt[s].flash = 255;
  uint8_t bed = max<uint8_t>(40, scale8(audio_gate(p.bass, st.sensitivity, 12), 150));
  uint8_t level = max<uint8_t>(bed, rt[s].flash);
  fill_strand(s, strand_color(p, st, 0, scale8_video(combined_brightness(p, st), level)));
  rt[s].flash = scale8(rt[s].flash, (uint8_t)map(st.strobe, 0, 255, 150, 235));
}

// Techno/house. Each hit launches a ring from the centre that travels outward.
void render_kick_ripple(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 110, 40));
  if (newBeat) {
    uint8_t slot = rt[s].phase & 3U;
    rt[s].ring[slot] = 1;
    rt[s].ringHue[slot] = (uint8_t)(rt[s].phase * 29U);
    rt[s].phase++;
  }
  uint8_t b = combined_brightness(p, st);
  uint16_t half = (n + 1U) / 2U;
  uint16_t step = len_scale(n, (uint16_t)(1U + st.speed / 64U));
  const uint16_t baseWidth = (uint16_t)(1U + st.density / 64U);
  uint16_t width = len_scale(n, baseWidth);
  int centerL = (int)((n - 1U) / 2U);
  int centerR = (int)(n / 2U);
  for (uint8_t k = 0; k < 4; k++) {
    if (rt[s].ring[k] == 0) continue;
    uint16_t r = rt[s].ring[k] - 1U;
    if (r >= half + width) { rt[s].ring[k] = 0; continue; }
    for (uint16_t w = 0; w < width; w++) {
      if (r < w) break;
      uint16_t rr = r - w;
      if (rr >= half) continue;
      Rgb c = strand_color(p, st, rt[s].ringHue[k], scale8_video(b, (uint8_t)(255U - ((uint32_t)w * 50U * baseWidth) / width)));
      if (centerL - (int)rr >= 0) set_pixel(s, (uint16_t)(centerL - rr), c);
      if (centerR + (int)rr < n) set_pixel(s, (uint16_t)(centerR + rr), c);
    }
    rt[s].ring[k] += step;
  }
}

// Drum & bass. Each hit slams a lit block into a random slot (a chopped break);
// hi-hats (treble) add white sparks.
void render_break_chopper(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 120, 45));
  uint8_t b = combined_brightness(p, st);
  if (newBeat) {
    uint16_t seg = max<uint16_t>(3, ((uint32_t)n * (1U + st.density / 64U)) / 16U);
    if (seg > n) seg = n;
    uint16_t start = (uint16_t)random(n - seg + 1U);
    uint8_t hue = (uint8_t)((rt[s].phase++ % 3U) * 85U);
    for (uint16_t i = 0; i < seg; i++) set_pixel(s, start + i, strand_color(p, st, hue, b));
  }
  uint8_t hats = audio_gate(p.treble, st.sensitivity, 30);
  uint8_t sparks = (uint8_t)(((uint16_t)hats * (1U + st.twinkle / 64U)) / 128U);
  for (uint8_t k = 0; k < sparks; k++) {
    uint8_t v = scale8_video(b, (uint8_t)random(120, 256));
    add_pixel(s, (uint16_t)random(n), {v, v, v});
  }
}

// Acid. A filter-sweep look: a resonant peak glides with the mid level (like a 303
// cutoff), a dim tail fills the strand below it, and an LFO plus the highs wobble the colour.
void render_acid_squelch(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t mid = audio_gate(p.mid, st.sensitivity, 10);
  int32_t target = (int32_t)mid * 257;
  rt[s].aux = (uint16_t)((int32_t)rt[s].aux + (target - (int32_t)rt[s].aux) / 4);  // glide
  uint16_t pos = (uint16_t)(((uint32_t)rt[s].aux * (n - 1U)) / 65535U);
  uint8_t lfo = triwave8((uint8_t)((millis() * (10U + st.speed)) / 256U));
  uint8_t hueOff = (uint8_t)(lfo / 5U + p.treble / 6U);
  uint8_t b = combined_brightness(p, st);
  for (uint16_t i = 0; i < pos; i++) {
    uint8_t v = (uint8_t)(30U + ((uint32_t)i * 70U) / max<uint16_t>(1, pos));
    set_pixel(s, oriented_index(i, n, st.direction), strand_color(p, st, hueOff, scale8_video(b, v)));
  }
  uint8_t width = (uint8_t)(1U + st.density / 48U);
  for (int16_t w = -(int16_t)width; w <= (int16_t)width; w++) {
    int32_t i = (int32_t)pos + w;
    if (i < 0 || i >= n) continue;
    uint8_t v = (uint8_t)(255U - (uint16_t)abs(w) * 180U / (width + 1U));
    Rgb c = strand_color(p, st, (uint8_t)(hueOff + 20U), scale8_video(b, v));
    if (w == 0 && st.twinkle > 128) c = {scale8_video(b, v), scale8_video(b, v), scale8_video(b, v)};  // white-hot core
    set_pixel(s, oriented_index((uint16_t)i, n, st.direction), c);
  }
}

// General. A classic strobe: flashes at the rig's max rate whenever Strobe is on.
// Decay sets the flash length (0 = one frame, the sharpest).
void render_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  if (count_for(s) == 0) return;
  if (full_flash_allowed(s, p, millis())) rt[s].flash = 255;
  fill_strand(s, strobe_rgb(st, scale8_video(combined_brightness(p, st), rt[s].flash)));
  rt[s].flash = scale8(rt[s].flash, (uint8_t)map(st.strobe, 0, 255, 0, 200));
}

// General. Dark in quiet parts, a flash on each kick, and flat-out strobing at the
// max rate while the music is loud (drops). Audio response sets how loud is "loud".
void render_drop_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  if (count_for(s) == 0) return;
  bool peak = energy_mix(p) >= (uint8_t)map(st.sensitivity, 0, 255, 230, 90);
  if ((peak || newBeat) && full_flash_allowed(s, p, millis())) rt[s].flash = 255;
  fill_strand(s, strobe_rgb(st, scale8_video(combined_brightness(p, st), rt[s].flash)));
  rt[s].flash = scale8(rt[s].flash, (uint8_t)map(st.strobe, 0, 255, 0, 200));
}

// The Strobe button (owner, 2026-11-09): while packet.fullStrobe is set, the whole strip flashes on
// every kick, over whatever look each strand runs, in the strobe colour and strobe brightness. Through
// the strobe gate like any flash, so Flashing looks and the max rate apply. Nothing when Strobe is off.
// (Until then this was Kick Flash, with its own strength in packet.kickFlash; that byte is now unused.)
static uint8_t kickFlashLevel = 0;

void update_kick_flash(const AudioLedPacket &p, bool newBeat) {
  if (kickFlashLevel) kickFlashLevel = scale8(kickFlashLevel, 110);
  if (p.fullStrobe && newBeat && strobe_gate_allow(strobeGate, p.flags, millis())) kickFlashLevel = 255;
}

void apply_kick_flash(uint8_t s, const AudioLedPacket &p) {
  if (kickFlashLevel < 8) return;
  uint16_t n = count_for(s);
  uint8_t v = scale8_video(p.globalBrightness, kickFlashLevel);
  // DnB Split Original keeps the plain white flash it had before strobe colours existed.
  const Rgb c = p.strand[s].mode == DNB_SPLIT_ORIGINAL_MODE ? Rgb{v, v, v} : strobe_rgb(p.strand[s], v);
  for (uint16_t i = 0; i < n; i++) add_pixel(s, i, c);
}

// Phase 15: SoundSwitch cells. The S3 picks this mode while a cell fixture drives the
// strip; each LED takes its colour from the cell frame, blended between cell centres
// (density bit 7 gives hard-edged cells; bit 6 splits the cells across the active strands, so
// strand k shows its share and the beam runs across all of them; bits 0-3 space the cells out
// with dark gaps). Reverse direction flips the strand.
// The strand fields carry the Lumina FX cues from the S3 on top:
//   speed = beam position, sensitivity = beam level (0 = none), twinkle = UV sparkle,
//   strobe = kick look on each hit: 0 none, 1 ripple, 2 glitter, 3 pump*, 4 shatter, 5 comet, 6 duck*,
//            7 ping-pong* (* = through the rig strobe gate: only with Strobe on, never past the max rate).
cells::Frame cellSnapshot = {};

void render_cells(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  uint8_t level = combined_brightness(p, st);
  // density bits 0-3: spaced cells, the dark gap between cells in sixteenths of a cell (0 = none).
  // Spaced cells are solid blocks (the DJs like dark space between lit parts).
  const uint8_t gap16 = st.density & 0x0F;
  const bool smooth = (st.density & 0x80) == 0 && gap16 == 0;
  const bool rev = st.direction == 1;
  const uint32_t now = millis();
  const uint8_t strands = p.activeStrands < 1 ? 1 : (p.activeStrands > MAX_STRANDS ? MAX_STRANDS : p.activeStrands);
  const bool split = (st.density & 0x40) && strands > 1 && s < strands && cellSnapshot.total >= strands;
  const uint8_t first = split ? (uint8_t)((uint16_t)s * cellSnapshot.total / strands) : 0;
  const uint8_t count = split ? (uint8_t)((uint16_t)(s + 1) * cellSnapshot.total / strands - first) : cellSnapshot.total;
  if (st.strobe == 6) {  // duck: the colours dip on the kick and swell back (sidechain feel)
    if (newBeat && flash_allowed(s, p, now)) rt[s].flash = 255;
    level = scale8_video(level, (uint8_t)(255U - scale8(rt[s].flash, 225)));
    rt[s].flash = scale8(rt[s].flash, 215);
  }
  // Colour mood: SoundSwitch's colours are folded into the mood's range like any look colour.
  const bool moodOn = mood::active(st);
  const uint8_t moodDrift = mood::drift(p.ms), moodSpan = mood::span(st);
  for (uint16_t i = 0; i < n; i++) {
    const uint16_t along = rev ? (uint16_t)(n - 1U - i) : i;
    if (cells::in_gap(count, along, n, gap16)) {
      set_pixel(s, i, {0, 0, 0});
      continue;
    }
    uint8_t r, g, b;
    cells::sample_range(cellSnapshot, first, count, along, n, smooth, r, g, b);
    Rgb c = {scale8_video(r, level), scale8_video(g, level), scale8_video(b, level)};
    if (moodOn && (c.r | c.g | c.b)) {
      uint8_t h, sat, v;
      rgb_to_hsv8(c, h, sat, v);
      c = hsv_to_rgb(mood::fold(st.moodFrom, moodSpan, h, moodDrift), max<uint8_t>(sat, mood::kMinSaturation), v);
    }
    set_pixel(s, i, c);
  }
  const uint8_t gb = p.globalBrightness;  // overlays respect the rig brightness cap

  // Beam: a soft white spot where SoundSwitch's Pan/Tilt point.
  // Split: the beam position covers all strands end to end, so it shows only on the strand it is over.
  const uint32_t beamAlong = (uint32_t)st.speed * (split ? strands : 1U);
  const uint32_t beamLo = split ? (uint32_t)s * 255U : 0;
  if (st.sensitivity && beamAlong >= beamLo && beamAlong <= beamLo + 255U) {
    const uint16_t pos = (uint16_t)(((beamAlong - beamLo) * (n - 1U)) / 255U);
    const uint16_t centre = rev ? (uint16_t)(n - 1U - pos) : pos;
    const uint16_t half = max<uint16_t>(2, n / 24);
    const uint8_t top = scale8_video(gb, st.sensitivity);
    for (int32_t d = -(int32_t)half; d <= (int32_t)half; d++) {
      const int32_t i = (int32_t)centre + d;
      if (i < 0 || i >= (int32_t)n) continue;
      const uint8_t v = scale8_video(top, (uint8_t)(255 - (abs(d) * 255) / (half + 1)));
      add_pixel(s, (uint16_t)i, {v, v, v});
    }
  }

  // UV sparkle burst.
  if (st.twinkle) {
    const uint16_t count = len_scale(n, (uint16_t)(1U + st.twinkle / 16U));
    for (uint16_t k = 0; k < count; k++) {
      const uint8_t v = scale8_video(gb, (uint8_t)random(120, 256));
      add_pixel(s, (uint16_t)random(n), {v, v, v});
    }
  }

  // Kick look on each hit (the S3 decides whether kicks are on right now), in the strobe colour.
  if (st.strobe == 1) {  // ripple: rings from the centre (not a flash)
    if (newBeat) {
      const uint8_t slot = rt[s].phase & 3U;
      rt[s].ring[slot] = 1;
      rt[s].phase++;
    }
    const uint16_t half = (n + 1U) / 2U, step = len_scale(n, 3), width = len_scale(n, 2);
    const int cl = (int)((n - 1U) / 2U), cr = (int)(n / 2U);
    for (uint8_t k = 0; k < 4; k++) {
      if (rt[s].ring[k] == 0) continue;
      const uint16_t r = rt[s].ring[k] - 1U;
      if (r >= half + width) { rt[s].ring[k] = 0; continue; }
      for (uint16_t w = 0; w < width && w <= r; w++) {
        const uint16_t rr = r - w;
        if (rr >= half) continue;
        const Rgb c = strobe_rgb(st, scale8_video(gb, (uint8_t)(255U - (w * 200U) / width)));
        if (cl - (int)rr >= 0) add_pixel(s, (uint16_t)(cl - rr), c);
        if (cr + (int)rr < (int)n) add_pixel(s, (uint16_t)(cr + rr), c);
      }
      rt[s].ring[k] += step;
    }
  } else if (st.strobe == 2) {  // glitter: a burst of sparkles that thins out
    if (newBeat) rt[s].sparkle = 255;
    if (rt[s].sparkle > 8) {
      const uint16_t count = len_scale(n, (uint16_t)(rt[s].sparkle / 12U));
      for (uint16_t k = 0; k < count; k++) {
        add_pixel(s, (uint16_t)random(n), strobe_rgb(st, scale8_video(gb, (uint8_t)random(150, 256))));
      }
      rt[s].sparkle = scale8(rt[s].sparkle, 200);
    }
  } else if (st.strobe == 3) {  // pump: a whole-strand flash, through the strobe gate
    if (newBeat && flash_allowed(s, p, now)) rt[s].flash = 255;
    if (rt[s].flash > 8) {
      const Rgb c = strobe_rgb(st, scale8_video(gb, scale8(rt[s].flash, 160)));
      for (uint16_t i = 0; i < n; i++) add_pixel(s, i, c);
      rt[s].flash = scale8(rt[s].flash, 170);
    }
  } else if (st.strobe == 4) {  // shatter: a few random shards that fade (partial, not a flash)
    if (newBeat) {
      rt[s].aux = (uint16_t)random(1, 65535);
      rt[s].sparkle = 255;
    }
    if (rt[s].sparkle > 8) {
      uint32_t x = rt[s].aux;  // same shard positions every frame until the next kick
      const uint16_t len = len_scale(n, 4);
      const Rgb c = strobe_rgb(st, scale8_video(gb, rt[s].sparkle));
      for (uint8_t k = 0; k < 4; k++) {
        x = x * 1103515245UL + 12345UL;
        const uint16_t start = (uint16_t)((x >> 8) % (n > len ? n - len : 1));
        for (uint16_t i = 0; i < len && start + i < n; i++) add_pixel(s, start + i, c);
      }
      rt[s].sparkle = scale8(rt[s].sparkle, 190);
    }
  } else if (st.strobe == 5) {  // comet: a comet across the strip, alternating direction each kick
    const uint16_t tail = len_scale(n, 14), step = len_scale(n, 7);
    if (newBeat) {
      rt[s].pos = 1;  // 0 = no comet
      rt[s].phase ^= 1;
    }
    if (rt[s].pos) {
      const uint16_t head = rt[s].pos - 1U;
      for (uint16_t k = 0; k < tail && k <= head; k++) {
        const uint16_t i = head - k;
        if (i >= n) continue;
        const Rgb c = strobe_rgb(st, scale8_video(gb, (uint8_t)(255U - (k * 240U) / tail)));
        add_pixel(s, (rt[s].phase & 1) ? (uint16_t)(n - 1U - i) : i, c);
      }
      rt[s].pos = head >= n + tail ? 0 : (uint16_t)(rt[s].pos + step);  // run until the tail has left the strip
    }
  } else if (st.strobe == 7) {  // ping-pong: alternate halves flash, through the strobe gate
    if (newBeat && flash_allowed(s, p, now)) {
      rt[s].flash = 255;
      rt[s].phase ^= 1;
    }
    if (rt[s].flash > 8) {
      const Rgb c = strobe_rgb(st, scale8_video(gb, rt[s].flash));
      const uint16_t half = n / 2U, from = (rt[s].phase & 1) ? half : 0, to = (rt[s].phase & 1) ? n : half;
      for (uint16_t i = from; i < to; i++) add_pixel(s, i, c);
      rt[s].flash = scale8(rt[s].flash, 150);
    }
  }
}

// ============================================================
// Halloween (owner, 2026-10-27): casual themed looks with no audio response and fixed
// colours. Speed sets the pace. Nothing here flashes.
// ============================================================
static uint8_t hw[MAX_STRANDS][160];  // per-segment state (candle level, bubble life)

static uint16_t hw_segments(uint16_t n, uint16_t seg) { return (uint16_t)min<uint32_t>(160U, (n + seg - 1U) / seg); }

// Rows of warm orange candles, each flickering on its own like a jack-o-lantern.
void render_candle_flicker(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t b = combined_brightness(p, st);
  const uint16_t seg = max<uint16_t>(3, len_scale(n, 6)), segs = hw_segments(n, seg);
  const int16_t jitter = (int16_t)(12 + st.speed / 6);
  for (uint16_t k = 0; k < segs; k++) {
    int16_t v = hw[s][k] ? hw[s][k] : 200;
    v += (int16_t)random(-jitter, jitter + 1);
    if (random(0, 120) == 0) v = 70;  // a draught: one candle gutters for a moment
    v = constrain(v, 70, 255);
    hw[s][k] = (uint8_t)v;
    const Rgb c = hsv_to_rgb((uint8_t)(10 + (v - 70) / 12), 255, scale8_video(b, (uint8_t)v));  // dimmer = redder
    for (uint16_t i = k * seg; i < (k + 1U) * seg && i < n; i++) set_pixel(s, i, c);
  }
}

// A dark cauldron glow with green and purple bubbles that swell and pop.
void render_witchs_brew(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t b = combined_brightness(p, st);
  const uint16_t seg = max<uint16_t>(3, len_scale(n, 5)), segs = hw_segments(n, seg);
  const uint8_t chance = (uint8_t)(140U - st.speed / 2U);  // faster = more bubbles
  for (uint16_t k = 0; k < segs; k++) {
    uint8_t &life = hw[s][k];
    if (life == 0 && random(0, chance) == 0) life = 255;
    const uint8_t step = (uint8_t)(3U + st.speed / 40U);
    life = life > step ? (uint8_t)(life - step) : 0;
    const uint8_t swell = life > 128 ? (uint8_t)((255 - life) * 2) : (uint8_t)(life * 2);  // rise, then pop
    const uint8_t hue = (k % 3U == 0) ? 200 : 86;  // purple or green
    const Rgb base = hsv_to_rgb(hue, 255, scale8_video(b, 18));
    const Rgb bubble = hsv_to_rgb(hue, 230, scale8_video(b, swell));
    for (uint16_t i = k * seg; i < (k + 1U) * seg && i < n; i++) set_pixel(s, i, life ? bubble : base);
  }
}

// Pale blue-white ghosts drifting along a dark strip, fading in and out.
void render_ghost_drift(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  const uint8_t b = combined_brightness(p, st);
  const uint32_t t = millis();
  const uint16_t width = max<uint16_t>(3, len_scale(n, 9));
  for (uint8_t g = 0; g < 3; g++) {
    const uint32_t travel = (t * (8U + st.speed / 8U + g * 3U)) / 1000U;
    const uint16_t centre = (uint16_t)((travel + (uint32_t)g * n / 3U) % n);
    const uint8_t fade = triwave8((uint8_t)(t / (28U + g * 9U) + g * 85U));
    for (int32_t d = -(int32_t)width; d <= (int32_t)width; d++) {
      const uint8_t v = scale8_video(scale8_video(b, fade), (uint8_t)(255 - (abs(d) * 255) / (width + 1)));
      add_pixel(s, wrap_index((int32_t)centre + d, n), hsv_to_rgb(150, 70, v));
    }
  }
}

// Orange pumpkins with green stems, gently breathing.
void render_pumpkin_patch(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t b = combined_brightness(p, st);
  const uint16_t size = max<uint16_t>(4, len_scale(n, 8));  // pumpkin + stem + gap
  const uint8_t breath = (uint8_t)(150U + triwave8((uint8_t)(millis() / (40U - st.speed / 8U))) * 105U / 255U);
  for (uint16_t i = 0; i < n; i++) {
    const uint16_t k = i % size;
    if (k < size - 2U) set_pixel(s, i, hsv_to_rgb(20, 255, scale8_video(b, breath)));  // pumpkin
    else if (k == size - 2U) set_pixel(s, i, hsv_to_rgb(86, 255, scale8_video(b, 120)));  // stem
    else set_pixel(s, i, {0, 0, 0});
  }
}

// Deep red drops gather at the start of the strip, then run down it with a trail.
void render_blood_drip(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t b = combined_brightness(p, st);
  fade_strand(s, 30);
  for (uint16_t i = 0; i < min<uint16_t>(n, len_scale(n, 3)); i++) set_pixel(s, i, hsv_to_rgb(0, 255, scale8_video(b, 90)));  // the pool
  if (random(0, 90 - st.speed / 4) == 0) {  // a new drop
    for (uint8_t k = 0; k < 4; k++) if (rt[s].ring[k] == 0) { rt[s].ring[k] = 1; rt[s].ringHue[k] = 0; break; }
  }
  for (uint8_t k = 0; k < 4; k++) {
    if (rt[s].ring[k] == 0) continue;
    const uint16_t pos = rt[s].ring[k] - 1U;
    if (pos >= n) { rt[s].ring[k] = 0; continue; }
    add_pixel(s, pos, hsv_to_rgb(0, 255, b));
    rt[s].ringHue[k] = (uint8_t)min<uint16_t>(255, rt[s].ringHue[k] + 1U);  // drops speed up as they run
    rt[s].ring[k] += (uint16_t)(1U + len_scale(n, (uint16_t)(rt[s].ringHue[k] / 24U)));
  }
}

// Slow purple and green fog rolling along the strip.
void render_graveyard_fog(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t b = combined_brightness(p, st);
  const uint32_t t = millis();
  const uint16_t ta = (uint16_t)(t / max<uint16_t>(20, 90U - st.speed / 4U)), tb = (uint16_t)(t / max<uint16_t>(26, 130U - st.speed / 4U));
  const uint8_t span = (uint8_t)max<uint16_t>(1, 255U * 3U / max<uint16_t>(1, n));  // about 3 fog banks per strip
  for (uint16_t i = 0; i < n; i++) {
    const uint8_t a = triwave8((uint8_t)(i * span + ta));
    const uint8_t c = triwave8((uint8_t)(i * span * 2U - tb));
    const Rgb purple = hsv_to_rgb(200, 220, scale8_video(b, scale8(a, a)));
    const Rgb green = hsv_to_rgb(86, 200, scale8_video(b, scale8(c, c) / 2U));
    set_pixel(s, i, add_color(purple, green));
  }
}

// ============================================================
// Club (DJ feedback, 2026-10-31): built around the DJs' two favourites, Hyper Strobe (little
// strobes, then an intense one) and DnB Split (spaced, chopped band meters). Dark space between
// the lit parts; never the whole strip at once. Bursts go through the rig strobe gate like any
// flash, in the strobe colour with look-coloured edges.
// ============================================================

// Energy spike for the Hyper modes: how far the jump in overall energy since the last packet went
// past the threshold (0 = none). Audio response sets the threshold: higher fires more often.
static uint8_t club_spike(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint8_t energy = energy_mix(p);
  const int16_t delta = (int16_t)energy - (int16_t)rt[s].prevEnergy;
  rt[s].prevEnergy = energy;
  const int16_t threshold = (int16_t)map(st.sensitivity, 0, 255, 60, 12);
  return delta > threshold ? (uint8_t)min<int16_t>(255, delta - threshold) : 0;
}

// Little hits start a dim burst, big ones a full-brightness one.
static uint8_t club_level(uint8_t spike) { return (uint8_t)min<uint16_t>(255, 130U + spike * 4U); }

// Decay sets how long a burst glows (0 = one sharp blink).
static uint8_t club_keep(const StrandSettings &st) { return (uint8_t)map(st.strobe, 0, 255, 80, 215); }

static void add_burst(uint8_t s, uint16_t pos, uint8_t level) {
  StrandRuntime &r = rt[s];
  r.burstPos[r.burstNext] = pos;
  r.burstLevel[r.burstNext] = level;
  r.burstNext = (uint8_t)((r.burstNext + 1U) % 12U);
}

// One little strobe from `start`: strobe-colour core, look-coloured outer LEDs when 3 or wider.
static void draw_burst(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, uint16_t n, int32_t start, uint16_t width, uint8_t level) {
  const uint8_t b = scale8_video(combined_brightness(p, st), level);
  const Rgb core = strobe_rgb(st, b), edge = strand_color(p, st, 110, b);
  for (uint16_t k = 0; k < width; k++) {
    const int32_t i = start + k;
    if (i < 0 || i >= (int32_t)n) continue;
    set_pixel(s, (uint16_t)i, (width >= 3 && (k == 0 || k == width - 1U)) ? edge : core);
  }
}

// Little strobes scattered along the strip on energy spikes; a big hit throws a whole volley.
void render_hyper_scatter(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  const uint8_t spike = club_spike(s, p, st);
  if (spike && flash_allowed(s, p, millis())) {
    const uint16_t base = (uint16_t)(1U + spike / 14U + st.density / 96U);  // more bursts for bigger hits
    const uint16_t count = min<uint16_t>(12, len_scale(n, base));
    for (uint16_t k = 0; k < count; k++) add_burst(s, (uint16_t)random(n), club_level(spike));
  }
  clear_strand(s);
  const uint16_t width = len_scale(n, (uint16_t)(2U + st.density / 64U));  // 2-5 LEDs per 150
  const uint8_t keep = club_keep(st);
  for (uint8_t k = 0; k < 12; k++) {
    uint8_t &level = rt[s].burstLevel[k];
    if (level < 10) continue;
    draw_burst(s, p, st, n, (int32_t)rt[s].burstPos[k] - (int32_t)(width / 2U), width, level);
    level = scale8(level, keep);
  }
}

// Evenly spaced slots with dark gaps; spikes flash a few slots, a big hit the whole grid.
// Sparkle keeps the slots faintly lit between hits (0 = dark).
void render_hyper_grid(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  const uint16_t slots = min<uint16_t>(48, max<uint16_t>(2, len_scale(n, (uint16_t)(8U + st.density / 32U))));  // 8-15 per 150
  const uint16_t cell = max<uint16_t>(1, n / slots);
  const uint16_t lit = max<uint16_t>(1, cell / 3U);  // a third of each slot lit
  const uint8_t spike = club_spike(s, p, st);
  if (spike && flash_allowed(s, p, millis())) {
    if (spike >= 40) {
      for (uint16_t k = 0; k < slots; k++) rt[s].slotLevel[k] = 255;
    } else {
      for (uint8_t k = 0; k < 1U + spike / 12U; k++) rt[s].slotLevel[random(slots)] = club_level(spike);
    }
  }
  clear_strand(s);
  const uint8_t keep = club_keep(st);
  const uint8_t pilot = scale8_video(combined_brightness(p, st), scale8(st.twinkle, 90));
  for (uint16_t k = 0; k < slots; k++) {
    uint8_t &level = rt[s].slotLevel[k];
    const uint16_t start = (uint16_t)(k * cell + (cell - lit) / 2U);
    if (level >= 10) {
      draw_burst(s, p, st, n, start, lit, level);
      level = scale8(level, keep);
    } else if (st.twinkle) {
      const Rgb c = strand_color(p, st, (uint8_t)(k * 9U), pilot);
      for (uint16_t i = start; i < start + lit && i < n; i++) set_pixel(s, i, c);
    }
  }
}

// Bursts in mirrored pairs around the centre; bigger hits land further out, a big hit adds a
// second pair near the middle.
void render_hyper_mirror(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  const uint16_t half = max<uint16_t>(1, n / 2U);
  const uint8_t spike = club_spike(s, p, st);
  if (spike && flash_allowed(s, p, millis())) {
    const uint32_t reach = ((uint32_t)min<uint8_t>(spike, 60) * (half - 1U)) / 60U + random(max<uint16_t>(1, half / 8U));
    add_burst(s, (uint16_t)min<uint32_t>(half - 1U, reach), club_level(spike));  // distance from the centre
    if (spike >= 40) add_burst(s, (uint16_t)random(max<uint16_t>(1, half / 4U)), 255);
  }
  clear_strand(s);
  const uint16_t width = len_scale(n, (uint16_t)(2U + st.density / 64U));
  const uint8_t keep = club_keep(st);
  const int32_t cl = (int32_t)((n - 1U) / 2U), cr = (int32_t)(n / 2U);
  for (uint8_t k = 0; k < 12; k++) {
    uint8_t &level = rt[s].burstLevel[k];
    if (level < 10) continue;
    const int32_t d = rt[s].burstPos[k];
    draw_burst(s, p, st, n, cl - d - (int32_t)width + 1, width, level);
    draw_burst(s, p, st, n, cr + d, width, level);
    level = scale8(level, keep);
  }
}

// DnB Split folded around the centre: bass meters grow out from the middle, then mids, then highs
// toward the ends, each chopped into scrolling tiles (same tiles as DnB Split).
void render_split_mirror(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  const uint16_t half = max<uint16_t>(1, n / 2U);
  const uint8_t values[3] = {eq_apply(p.bass, p.eq[0]), eq_apply(p.mid, p.eq[1]), eq_apply(p.treble, p.eq[2])};  // split-look EQ
  const uint8_t tile = max<uint8_t>(1, (uint8_t)map(st.density, 0, 255, 1, 5));
  const uint32_t scroll = millis() * (10U + st.speed) / 700U;
  const int32_t cl = (int32_t)((n - 1U) / 2U), cr = (int32_t)(n / 2U);
  uint16_t start = 0;  // distance from the centre
  for (uint8_t band = 0; band < 3; band++) {
    const uint16_t end = (uint16_t)(((uint32_t)(band + 1U) * half) / 3U);
    const uint16_t width = max<uint16_t>(1, end - start);
    const uint8_t level = audio_gate(values[band], st.sensitivity, 12);
    const uint16_t lit = ((uint32_t)level * width) / 255U;
    const Rgb c = strand_color(p, st, (uint8_t)(band * 82U), scale8_video(combined_brightness(p, st), level));
    for (uint16_t i = 0; i < lit; i++) {
      if (((i + scroll) / tile) & 1U) continue;
      const int32_t d = (int32_t)start + i;
      if (cl - d >= 0) set_pixel(s, (uint16_t)(cl - d), c);
      if (cr + d < (int32_t)n) set_pixel(s, (uint16_t)(cr + d), c);
    }
    start = end;
  }
}

// Short spaced meters along the whole strip, bass / mid / high repeating; each fills from its
// own centre with its band's level.
void render_split_blocks(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  const uint16_t blocks = max<uint16_t>(3, len_scale(n, (uint16_t)(6U + st.density / 43U)));  // 6-11 per 150
  const uint16_t cell = max<uint16_t>(1, n / blocks);
  const uint16_t len = max<uint16_t>(1, (uint16_t)((cell * 2U) / 3U));  // a third of each block stays dark
  const uint8_t values[3] = {p.bass, p.mid, p.treble};
  const uint8_t base = combined_brightness(p, st);
  for (uint16_t k = 0; k < blocks; k++) {
    const uint8_t band = (uint8_t)(k % 3U);
    const uint8_t level = audio_gate(values[band], st.sensitivity, 12);
    const uint16_t lit = ((uint32_t)level * len) / 255U;
    if (!lit) continue;
    const Rgb c = strand_color(p, st, (uint8_t)(band * 82U), scale8_video(base, max<uint8_t>(60, level)));
    const uint16_t from = (uint16_t)(k * cell + (cell - len) / 2U + (len - lit) / 2U);
    for (uint16_t i = from; i < from + lit && i < n; i++) set_pixel(s, oriented_index(i, n, st.direction), c);
  }
}

// DnB Split with a little strobe at the top of each meter on energy spikes.
void render_split_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  const uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  const uint8_t values[3] = {eq_apply(p.bass, p.eq[0]), eq_apply(p.mid, p.eq[1]), eq_apply(p.treble, p.eq[2])};  // split-look EQ
  const uint8_t tile = max<uint8_t>(1, (uint8_t)map(st.density, 0, 255, 1, 5));
  const uint32_t scroll = millis() * (10U + st.speed) / 700U;
  const uint8_t spike = club_spike(s, p, st);
  const bool fire = spike && flash_allowed(s, p, millis());
  const uint8_t keep = club_keep(st);
  const uint8_t base = combined_brightness(p, st);
  const uint16_t tipWidth = len_scale(n, 3);
  uint16_t start = 0;
  for (uint8_t band = 0; band < 3; band++) {
    const uint16_t end = (uint16_t)(((uint32_t)(band + 1U) * n) / 3U);
    const uint16_t width = max<uint16_t>(1, end - start);
    const uint8_t level = audio_gate(values[band], st.sensitivity, 12);
    const uint16_t lit = ((uint32_t)level * width) / 255U;
    const Rgb c = strand_color(p, st, (uint8_t)(band * 82U), scale8_video(base, level));
    for (uint16_t i = 0; i < lit; i++) {
      if (((i + scroll) / tile) & 1U) continue;
      set_pixel(s, oriented_index(start + i, n, st.direction), c);
    }
    if (fire) {  // burst slot `band` marks this meter's tip
      rt[s].burstPos[band] = (uint16_t)(start + lit);
      rt[s].burstLevel[band] = club_level(spike);
    }
    uint8_t &tipLevel = rt[s].burstLevel[band];
    if (tipLevel >= 10) {
      const Rgb t = strobe_rgb(st, scale8_video(base, tipLevel));
      const int32_t from = (int32_t)rt[s].burstPos[band] - (int32_t)(tipWidth / 2U);
      for (uint16_t k = 0; k < tipWidth; k++) {
        const int32_t i = from + k;
        if (i >= (int32_t)start && i < (int32_t)end) set_pixel(s, oriented_index((uint16_t)i, n, st.direction), t);
      }
      tipLevel = scale8(tipLevel, keep);
    }
    start = end;
  }
}

// 52 DnB Split Original (owner, 2026-11-09): the DJs found DnB Split under a colour mood too different.
// The same drawing as mode 28 (render_dnb_split, unchanged since Phase 14) with the mood switched off,
// so it always shows the strand's own hue and saturation; apply_kick_flash keeps its flash white.
void render_dnb_split_original(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  StrandSettings plain = st;
  plain.moodFrom = 0;
  plain.moodTo = 0;
  render_dnb_split(s, p, plain);
}
