#include "Effects.h"
#include "Color.h"
#include "EffectState.h"
#include "LedOutput.h"

StrandRuntime rt[MAX_STRANDS];
StrobeGate strobeGate = {0, 0};  // one flash timer for the whole rig (all strands)

// Strobe safety (docs/DECISIONS.md): every flash onset goes through here, so the
// sender's global strobe switch and max flash rate (packet flags) always apply.
static bool flash_allowed(uint8_t s, const AudioLedPacket &p, uint32_t now) {
  if (!strobe_gate_allow(strobeGate, p.flags, now)) return false;
  rt[s].lastTriggerMs = now;
  return true;
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

static Rgb strand_color(const AudioLedPacket &p, const StrandSettings &st, uint8_t offset = 0, uint8_t value = 255) {
  return hsv_to_rgb(st.hue + p.globalHue + offset, st.saturation, value);
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
  if (newBeat && flash_allowed(s, p, millis())) rt[s].flash = 255;
  fade_strand(s, st.strobe > 0 ? st.strobe : 80);
  if (rt[s].flash > 10) {
    fill_strand(s, strand_color(p, st, 0, combined_brightness(p, st)));
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

  for (uint8_t k = 0; k < chance; k++) {
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

  for (uint8_t k = 0; k < sparkles; k++) {
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
    set_pixel(s, wrap_index((int32_t)rt[s].aux + i, n), (i & 1U) ? strand_color(p, st, 110, b) : Rgb{b,b,b});
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
        add_pixel(s, (uint16_t)idx, (k & 1U) ? Rgb{0,(uint8_t)(b / 2U),b} : Rgb{b,b,b});
      }
    }
  }
}

void render_dnb_split(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  clear_strand(s);
  uint8_t values[3] = {p.bass, p.mid, p.treble};
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
