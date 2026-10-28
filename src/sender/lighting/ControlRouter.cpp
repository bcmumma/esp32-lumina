#include "ControlRouter.h"

#include <Preferences.h>

#include "Lumina16.h"
#include "CellFrame.h"
#include "FixtureLayout.h"
#include "FxLayer.h"
#include "../network/EspNowSender.h"
#include "../config/ConfigManager.h"
#include "../network/ArtNetReceiver.h"

ControlState control = {router::Source::Auto, router::Source::Audio, router::kDefaultStandaloneLevel, STROBE_DEFAULT_HZ,
                        false, 1, 0, 0, RIG_BRIGHTNESS_CAP_MAX, false, 1, true, true, true, false, false, 255, 3};

namespace {

// Separate small record so the main dual-slot config blob is untouched (docs/DECISIONS.md).
constexpr char kNamespace[] = "lumina_ctl";
constexpr char kKey[] = "rec";
constexpr uint32_t kMagic = 0x4C435431UL;  // "LCT1"
constexpr uint8_t kVersion = 3;  // v3 appends the FX settings; v1 and v2 records still load

struct ControlRecord {
  uint32_t magic;
  uint8_t version;
  uint8_t source;
  uint8_t autoFallback;
  uint8_t strobeHz;
  uint8_t standaloneLevel;
  uint8_t brightnessCap;
  uint8_t kickFlash;     // v2 reserved byte, 0 in older records = off
  uint8_t fixture;       // v2 reserved byte: 0 = LUMINA-16, 1..150 = N cells, 151.. = named layout (Phase 15)
  uint32_t crc32;
};

// v3 (Phase 15 FX): the v2 fields, then the FX settings. Stored separately sized, so a v2 record
// (sizeof(ControlRecord)) still loads and gets FX defaults.
struct ControlRecordV3 {
  ControlRecord base;  // base.crc32 unused in v3
  uint8_t fxFlags;     // bit 0 FX on, bits 1-3 kick look, bit 4 beam OFF, bit 5 sparkle OFF, bit 6 FX strobe OFF,
                       // bit 7 kicks always ("OFF" bits so older v3 records keep every look on)
  uint8_t beamStrength;
  uint8_t beamSpread;
  uint8_t flags2;      // bit 0 split cells across strands (was reserved, 0 = off)
  uint32_t crc32;
};

uint32_t record_crc_v3(const ControlRecordV3 &r) {
  return config_crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(ControlRecordV3, crc32));
}

uint8_t clamp_cap(int v) {
  return (uint8_t)(v < RIG_BRIGHTNESS_CAP_MIN ? RIG_BRIGHTNESS_CAP_MIN : (v > RIG_BRIGHTNESS_CAP_MAX ? RIG_BRIGHTNESS_CAP_MAX : v));
}

uint32_t record_crc(const ControlRecord &r) {
  return config_crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(ControlRecord, crc32));
}

router::Decision lastDecision = {router::Source::Audio, router::Reason::Selected, false, false};
bool haveDecision = false;
uint32_t switches = 0;
lumina16::Params lastParams = {};
bool haveParams = false;
uint32_t appliedPackets = 0;
uint8_t cellRgb[MAX_CELLS * 3];
uint32_t cellFrames = 0;
uint8_t heldBeatPulse = 0;  // bass-hit pulse is frozen while audio isn't the active source

// Phase 15 Lumina FX fixture.
fx::State fxState = {};
fx::Output fxOut = {};
uint32_t fxSeenRxMs = 0;
uint8_t fxPulse = 0;          // backup hit counter from FX Intensity edges (no audio)
uint32_t lastAudioMs = 0;     // last time the line input had signal
constexpr uint32_t kAudioSilentMs = 2000;

// Phase 15: when SoundSwitch last showed light, and its colour then (for HYBRID).
uint32_t lastSeenRxMs = 0;
uint32_t lastLitMs = 0;  // 0 = never lit
bool haveSswColour = false;
uint8_t sswHue = 0, sswSat = 0;

router::Config router_config() {
  return {control.source, router::sanitize_fallback(control.autoFallback), router::kDefaultTimeoutMs, control.standaloneLevel,
          router::kDefaultDarkMs, router::kDefaultHybridHoldMs};
}

// Phase 15: the current SoundSwitch fixture as a layout. False for LUMINA-16.
// Plain "N cells" is one fixture of N RGB cells; codes above 150 are named layouts.
bool current_layout(layout::Layout &out) {
  const uint8_t code = control.fixture;
  if (code == 0) return false;
  if (layout::is_preset_code(code)) {
    const layout::Layout *p = layout::preset(layout::preset_id(code));
    if (!p) return false;
    out = *p;
    return true;
  }
  out = {"custom", "", 1, code, false, false, false, false, false};
  return true;
}

uint8_t cellStrobe[MAX_CELLS];  // strobe value per strip cell (layouts with Strobe)

// Decodes the frame into cellRgb for the current layout. Returns strip cells, 0 on failure.
uint8_t decode_cells(const layout::Layout &l, const uint8_t *dmx, uint16_t length) {
  return layout::decode(l, dmx, length, control.address, cellRgb, cellStrobe) ? layout::strip_cells(l) : 0;
}

// Looks at a new frame once: is anything lit in the fixture's channels, and what colour leads?
void track_frame(const uint8_t *dmx, uint16_t length, uint32_t rxMs) {
  if (!dmx || rxMs == lastSeenRxMs) return;
  lastSeenRxMs = rxMs;
  uint8_t r = 0, g = 0, b = 0;
  bool lit = false;
  layout::Layout l;
  if (current_layout(l)) {
    const uint8_t n = decode_cells(l, dmx, length);
    if (!n) return;
    uint16_t best = 0;
    for (uint8_t c = 0; c < n; c++) {  // brightest cell leads the colour
      const uint8_t *q = cellRgb + c * 3;
      const uint16_t sum = (uint16_t)q[0] + q[1] + q[2];
      if (sum > best) { best = sum; r = q[0]; g = q[1]; b = q[2]; }
    }
    lit = best > 0;
  } else {
    lumina16::Params q;
    if (!lumina16::decode(dmx, length, control.address, q)) return;
    const uint16_t a = control.address - 1U;
    r = dmx[a + 8]; g = dmx[a + 9]; b = dmx[a + 10];
    lit = q.dimmer > 0 && (r || g || b);
  }
  if (!lit) return;
  lastLitMs = rxMs ? rxMs : 1;
  uint8_t v;
  lumina16::rgb_to_hsv(r, g, b, sswHue, sswSat, v);
  haveSswColour = true;
}

// Fixture Strobe channels (named layouts with "Strobe"; each strobes its fixture's last cell).
// Rig-wide and safety-first: strobing cells flash only while Strobe is enabled, never faster than
// the rig's max rate (the fastest sets the pace); with Strobe off they stay steady. Between
// flashes they go dark.
uint32_t lastCellFlashMs = 0;
uint32_t cellFlashUntilMs = 0;
uint32_t cellFlashes = 0;

bool apply_cell_strobe(uint8_t n, uint32_t nowMs) {
  if (!control.strobeEnabled) return false;
  uint8_t hz = 0;
  for (uint8_t c = 0; c < n; c++) {
    const uint8_t h = layout::strobe_hz(cellStrobe[c], control.strobeHz);
    if (h > hz) hz = h;
  }
  if (hz == 0) return false;
  if (lastCellFlashMs == 0 || nowMs - lastCellFlashMs >= 1000U / hz) {
    lastCellFlashMs = nowMs ? nowMs : 1;
    cellFlashUntilMs = nowMs + 40;  // longer than one packet interval, so every flash is sent
    cellFlashes++;
  }
  const bool on = (int32_t)(cellFlashUntilMs - nowMs) > 0;
  for (uint8_t c = 0; c < n; c++) {
    if (cellStrobe[c] >= 10 && !on) cellRgb[c * 3] = cellRgb[c * 3 + 1] = cellRgb[c * 3 + 2] = 0;
  }
  return true;
}

}  // namespace

const char *control_fixture_name() {
  static char buf[16];
  if (control.fixture == 0) return "LUMINA-16";
  if (layout::is_preset_code(control.fixture)) {
    const layout::Layout *l = layout::preset(layout::preset_id(control.fixture));
    return l ? l->name : "unknown";
  }
  snprintf(buf, sizeof(buf), "%u cells", control.fixture);
  return buf;
}

void load_control_settings() {
  Preferences store;
  ControlRecord r = {};
  ControlRecordV3 r3 = {};
  bool ok = false, v3 = false;
  if (store.begin(kNamespace, true)) {
    const size_t len = store.getBytesLength(kKey);
    if (len == sizeof(ControlRecordV3) && store.getBytes(kKey, &r3, sizeof(r3)) == sizeof(r3) && r3.base.magic == kMagic &&
        r3.base.version == 3 && r3.crc32 == record_crc_v3(r3)) {
      r = r3.base;
      ok = v3 = true;
    } else if (len == sizeof(ControlRecord)) {
      ok = store.getBytes(kKey, &r, sizeof(r)) == sizeof(r) && r.magic == kMagic && (r.version == 1 || r.version == 2) &&
           r.crc32 == record_crc(r);
    }
    ok = ok && r.source <= (uint8_t)router::Source::Hybrid;
    store.end();
  }
  if (ok) {
    control.source = (router::Source)r.source;
    control.autoFallback = router::sanitize_fallback((router::Source)r.autoFallback);
    control.strobeHz = strobe_clamp_hz(r.strobeHz);
    control.standaloneLevel = r.standaloneLevel;
    control.brightnessCap = r.version >= 2 ? clamp_cap(r.brightnessCap) : RIG_BRIGHTNESS_CAP_MAX;
    control.kickFlash = r.version >= 2 ? r.kickFlash : 0;
    const uint8_t code = layout::migrate_code(r.fixture);  // retired test layouts load as L1
    const bool known = code <= MAX_CELLS || layout::preset(layout::preset_id(code)) != nullptr;
    control.fixture = (r.version >= 2 && known) ? code : 0;
    if (v3) {
      control.fxEnabled = (r3.fxFlags & 1) != 0;
      control.fxHit = (uint8_t)((r3.fxFlags >> 1) & 7);
      control.beamOn = (r3.fxFlags & 0x10) == 0;
      control.sparkleOn = (r3.fxFlags & 0x20) == 0;
      control.fxStrobeOn = (r3.fxFlags & 0x40) == 0;
      control.kicksAlways = (r3.fxFlags & 0x80) != 0;
      control.splitStrands = (r3.flags2 & 1) != 0;
      control.beamStrength = r3.beamStrength;
      control.beamSpread = r3.beamSpread < 1 || r3.beamSpread > 8 ? 3 : r3.beamSpread;
    }
  }
  control.strobeEnabled = false;  // never restored: strobe is always off at boot
  Serial.printf("Control: source=%s fallback=%s strobe off (cap %u/s) standalone=%u brightnessCap=%u/255 fixture=%s (code %u) (%s)\n",
                router::name(control.source), router::name(control.autoFallback), control.strobeHz,
                control.standaloneLevel, control.brightnessCap, control_fixture_name(),
                control.fixture, ok ? "loaded" : "defaults");
}

bool save_control_settings() {
  ControlRecordV3 r3 = {};
  ControlRecord &r = r3.base;
  r.magic = kMagic;
  r.version = kVersion;
  r.source = (uint8_t)control.source;
  r.autoFallback = (uint8_t)router::sanitize_fallback(control.autoFallback);
  r.strobeHz = control.strobeHz;
  r.standaloneLevel = control.standaloneLevel;
  r.brightnessCap = clamp_cap(control.brightnessCap);
  r.kickFlash = control.kickFlash;
  r.fixture = control.fixture;
  r3.fxFlags = (uint8_t)((control.fxEnabled ? 1 : 0) | ((control.fxHit & 7) << 1) | (control.beamOn ? 0 : 0x10) |
                         (control.sparkleOn ? 0 : 0x20) | (control.fxStrobeOn ? 0 : 0x40) | (control.kicksAlways ? 0x80 : 0));
  r3.flags2 = control.splitStrands ? 1 : 0;
  r3.beamStrength = control.beamStrength;
  r3.beamSpread = control.beamSpread;
  r3.crc32 = record_crc_v3(r3);
  Preferences store;
  if (!store.begin(kNamespace, false)) return false;
  bool ok = store.putBytes(kKey, &r3, sizeof(r3)) == sizeof(r3);
  store.end();
  return ok;
}

void control_apply(AudioLedPacket &p, uint32_t nowMs) {
  p.flags = strobe_encode_flags(p.flags, control.strobeEnabled, control.strobeHz);

  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  const uint8_t *dmx = artnet_listen_dmx(length, lastRxMs);
  const bool haveFrame = dmx != nullptr && lastRxMs != 0;
  if (haveFrame) track_frame(dmx, length, lastRxMs);
  const uint32_t litAge = lastLitMs ? nowMs - lastLitMs : router::kNeverLit;
  const router::Decision d = router::decide(router_config(), haveFrame, haveFrame ? nowMs - lastRxMs : 0, litAge);

  if (haveDecision && (d.active != lastDecision.active || d.reason != lastDecision.reason)) {
    switches++;
    Serial.printf("Source: %s -> active %s (%s)\n", router::name(control.source), router::name(d.active),
                  router::reason_name(d.reason));
  }
  lastDecision = d;
  haveDecision = true;

  bool cellsActive = false;
  bool cellStrobing = false;
  bool fxActive = false;
  layout::Layout l;
  if (d.applyArtnet && current_layout(l)) {
    // Phase 15: a cell fixture or named layout. The receiver draws the cells (CELLS_MODE) along each strand.
    const uint8_t n = decode_cells(l, dmx, length);
    if (n) {
      // Lumina FX fixture: 5 channels right after the layout. Its Strobe strobes every cell.
      fxOut = fx::Output{};
      fxActive = l.fx;
      if (l.fx) {
        fx::Input in;
        const uint16_t fxAddress = control.address + layout::total_channels(l);
        if (fx::read(dmx, length, fxAddress, in)) {
          const bool newFrame = lastRxMs != fxSeenRxMs;
          fxSeenRxMs = lastRxMs;
          fxOut = fx::step(fxState, in, newFrame, {control.beamStrength, control.beamSpread}, nowMs);
          if (fxOut.hit) fxPulse++;
          if (!control.beamOn) fxOut.beamLevel = 0;
          if (!control.sparkleOn) fxOut.sparkle = 0;
          if (control.fxStrobeOn && fxOut.strobe >= 10)
            for (uint8_t c = 0; c < n; c++) if (cellStrobe[c] < fxOut.strobe) cellStrobe[c] = fxOut.strobe;
        }
      }
      cellStrobing = apply_cell_strobe(n, nowMs);
      // In CELLS_MODE the strand fields carry the FX cues to the receiver (see render_cells):
      // speed = beam position, sensitivity = beam level, twinkle = sparkle, strobe = kick effect.
      // Gated looks (pump, duck, ping-pong) pause while FX/fixture Strobe flashes, so the two can't add up.
      const bool gatedLook = control.fxHit == 3 || control.fxHit == 6 || control.fxHit == 7;
      const bool kicksNow = l.fx && (control.kicksAlways || fxOut.pulsing);
      const uint8_t hitStyle = (kicksNow && !(cellStrobing && gatedLook)) ? control.fxHit : 0;
      for (uint8_t s = 0; s < MAX_STRANDS; s++) {
        p.strand[s].mode = CELLS_MODE;
        p.strand[s].brightness = 255;                    // intensity is in the cell colours; the rig cap still applies
        // density bit 7: hard-edged blocks (else smooth); bit 6: split the cells across the active strands
        p.strand[s].density = (uint8_t)((l.hardEdges ? 0x80 : 0) | (control.splitStrands ? 0x40 : 0));
        p.strand[s].speed = fxOut.beamPos;
        p.strand[s].sensitivity = fxOut.beamLevel;
        p.strand[s].twinkle = fxOut.sparkle;
        p.strand[s].strobe = hitStyle;
      }
      queue_cell_frame(cellRgb, n);
      cellsActive = true;
      cellFrames++;
      appliedPackets++;
    }
  } else if (d.applyArtnet) {
    lumina16::Params params;
    if (lumina16::decode(dmx, length, control.address, params)) {
      lumina16::apply(p, params, 255);  // full dimmer range; the rig cap below limits it
      lastParams = params;
      haveParams = true;
      appliedPackets++;
    }
  }
  if (!cellsActive) queue_cell_frame(nullptr, 0);
  if (d.hybridColour && haveSswColour) {  // HYBRID: the audio look, in SoundSwitch's latest colour
    for (uint8_t s = 0; s < MAX_STRANDS; s++) {
      p.strand[s].hue = sswHue;
      p.strand[s].saturation = sswSat;
    }
    p.globalHue = 0;
  }
  if (d.standaloneLevels) router::apply_standalone(p, control.standaloneLevel);

  // Bass hits drive the lights when audio is the active source, and add Kick Flash over SoundSwitch cells.
  if (p.volume > 8) lastAudioMs = nowMs ? nowMs : 1;
  if (d.active == router::Source::Audio || d.active == router::Source::Hybrid || cellsActive) {
    // With no line input, SoundSwitch's FX Intensity edges stand in for the kicks.
    if (cellsActive && fxActive && (lastAudioMs == 0 || nowMs - lastAudioMs > kAudioSilentMs)) p.beatPulse = fxPulse;
    heldBeatPulse = p.beatPulse;
  } else {
    p.beatPulse = heldBeatPulse;
    p.beat = 0;
  }

  // Rig brightness cap (power budget), after every source.
  const uint8_t cap = clamp_cap(control.brightnessCap);
  if (p.globalBrightness > cap) p.globalBrightness = cap;

  // Receiver flashes Kick Flash only on bass hits, through the strobe gate. Muted while a fixture's own
  // Strobe channel is flashing, so the two can't add up past the rig's max rate.
  p.kickFlash = cellStrobing ? 0 : control.kickFlash;
}

void append_control_json(String &s, uint32_t nowMs) {
  uint16_t length = 0;
  uint32_t lastRxMs = 0;
  artnet_listen_dmx(length, lastRxMs);
  s += "{\"source\":\"" + String(router::name(control.source)) + "\"";
  s += ",\"active\":\"" + String(router::name(lastDecision.active)) + "\"";
  s += ",\"reason\":\"" + String(router::reason_name(lastDecision.reason)) + "\"";
  s += ",\"autoFallback\":\"" + String(router::name(control.autoFallback)) + "\"";
  s += ",\"timeoutMs\":" + String(router::kDefaultTimeoutMs);
  s += ",\"standaloneLevel\":" + String(control.standaloneLevel);
  s += ",\"switches\":" + String(switches);
  s += ",\"artnetAgeMs\":" + String(lastRxMs ? nowMs - lastRxMs : 0xFFFFFFFFUL);
  s += ",\"applyingArtnet\":";
  s += lastDecision.applyArtnet ? "true" : "false";
  s += ",\"appliedPackets\":" + String(appliedPackets);
  s += ",\"address\":" + String(control.address);
  s += ",\"brightnessCap\":" + String(control.brightnessCap);
  s += ",\"brightnessCapMin\":" + String(RIG_BRIGHTNESS_CAP_MIN);
  s += ",\"brightnessCapMax\":" + String(RIG_BRIGHTNESS_CAP_MAX);
  s += ",\"strobeEnabled\":";
  s += control.strobeEnabled ? "true" : "false";
  s += ",\"strobeHz\":" + String(control.strobeHz);
  s += ",\"strobeMaxHz\":" + String(STROBE_MAX_HZ);
  s += ",\"kickFlash\":" + String(control.kickFlash);
  s += ",\"fixture\":" + String(control.fixture);
  s += ",\"fixtureName\":\"" + String(control_fixture_name()) + "\"";
  {
    layout::Layout fl;
    s += ",\"fixtureCells\":" + String(current_layout(fl) ? layout::strip_cells(fl) : 0);
    s += ",\"fixtureChannels\":" + String(current_layout(fl) ? layout::total_channels(fl) : 16);
  }
  s += ",\"cellFrames\":" + String(cellFrames);
  s += ",\"cellPacketsSent\":" + String(cell_packets_sent);
  s += ",\"cellFlashes\":" + String(cellFlashes);
  s += ",\"rooms\":";
  append_rooms_json(s);
  {
    layout::Layout fl;
    s += ",\"fxEnabled\":" + String(current_layout(fl) && fl.fx ? 1 : 0);
  }
  s += ",\"fxHit\":" + String(control.fxHit) + ",\"splitStrands\":" + String(control.splitStrands ? 1 : 0);
  s += ",\"beamOn\":" + String(control.beamOn ? 1 : 0) + ",\"sparkleOn\":" + String(control.sparkleOn ? 1 : 0);
  s += ",\"fxStrobeOn\":" + String(control.fxStrobeOn ? 1 : 0) + ",\"kicksAlways\":" + String(control.kicksAlways ? 1 : 0);
  s += ",\"beamStrength\":" + String(control.beamStrength) + ",\"beamSpread\":" + String(control.beamSpread);
  {
    layout::Layout fl;
    s += ",\"fxAddress\":" + String(current_layout(fl) ? control.address + layout::total_channels(fl) : 0);
  }
  s += ",\"fxLive\":{\"beam\":" + String(fxOut.beamLevel) + ",\"pos\":" + String(fxOut.beamPos) +
       ",\"moving\":" + String(fxOut.moving ? 1 : 0) + ",\"pulsing\":" + String(fxOut.pulsing ? 1 : 0) +
       ",\"sparkle\":" + String(fxOut.sparkle) + ",\"edges\":" + String(fxState.edges) + "}";
  s += ",\"litAgeMs\":" + String(lastLitMs ? nowMs - lastLitMs : 0xFFFFFFFFUL);
  s += ",\"hybridHoldMs\":" + String(router::kDefaultHybridHoldMs);
  if (haveSswColour) s += ",\"sswHue\":" + String(sswHue) + ",\"sswSat\":" + String(sswSat);
  if (haveParams) {
    const lumina16::Params &q = lastParams;
    s += ",\"lumina16\":{\"dimmer\":" + String(q.dimmer) + ",\"mode\":" + String(q.mode) + ",\"speed\":" + String(q.speed) +
         ",\"sensitivity\":" + String(q.sensitivity) + ",\"density\":" + String(q.density) + ",\"strobe\":" + String(q.strobe) +
         ",\"direction\":" + String(q.direction) + ",\"variation\":" + String(q.variation) + ",\"hue\":" + String(q.hue) +
         ",\"saturation\":" + String(q.saturation) + ",\"value\":" + String(q.value) +
         ",\"brightness\":" + String(lumina16::brightness(q, 255)) + "}";
  }
  s += "}";
}

void append_fixture_list_json(String &s) {
  s += "[";
  for (uint8_t id = 1; id <= layout::preset_count(); id++) {
    const layout::Layout &l = *layout::preset(id);
    if (id > 1) s += ",";
    s += "{\"id\":" + String(id) + ",\"name\":\"" + String(l.name) + "\",\"mode\":\"" + String(l.mode) + "\"";
    s += ",\"fixtures\":" + String(l.fixtures) + ",\"cells\":" + String(l.cells);
    s += ",\"perFixture\":" + String(layout::channels_per_fixture(l)) + ",\"total\":" + String(layout::total_channels(l));
    s += ",\"stripCells\":" + String(layout::strip_cells(l));
    s += ",\"dimmer\":" + String(l.dimmer ? 1 : 0) + ",\"strobe\":" + String(l.strobe ? 1 : 0);
    s += ",\"mirror\":" + String(l.mirror ? 1 : 0) + ",\"hard\":" + String(l.hardEdges ? 1 : 0);
    s += ",\"fx\":" + String(l.fx ? 1 : 0) + ",\"fxAddress\":" + String(1 + layout::total_channels(l)) + "}";
  }
  s += "]";
}
