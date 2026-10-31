#include "LuminaWebServer.h"
#include <WiFi.h>
#include "IndexHtml.h"
#include "../config/ConfigManager.h"
#include "../lighting/LightingState.h"
#include "../audio/AudioAnalyzer.h"
#include "../network/WiFiManager.h"
#include "../network/EspNowSender.h"
#include "../network/ArtNetReceiver.h"
#include "../lighting/ControlRouter.h"
#include "FixtureLayout.h"
#include "../util/Clamp.h"
#include <Preferences.h>
#include <stdlib.h>

WebServer server(80);

uint32_t lastLoopUs = 0;
uint32_t maxLoopUs = 0;
uint32_t lastHttpUs = 0;
uint32_t maxHttpUs = 0;
uint32_t httpRequestCount = 0;
uint32_t lastLiveBytes = 0;

// Autosave (DJ feedback, 2026-10-31): look and setup changes are saved a few seconds after the
// last one, so nobody has to press Save. One write per burst of slider moves.
static constexpr uint32_t kAutosaveMs = 3000;
static uint32_t ledDirtyMs = 0;   // 0 = nothing unsaved
static uint32_t feelDirtyMs = 0;
static void mark_led_dirty() { ledDirtyMs = millis() | 1U; }
static void save_feel();

// // Web handlers
// ============================================================
static void no_cache() {
  server.client().setNoDelay(true);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
}

static void handle_root() {
  no_cache();
  server.send_P(200, "text/html", INDEX_HTML);
}

static void apply_float_arg(const char *name, float &value, float lo, float hi) {
  if (server.hasArg(name)) value = clamp_float(server.arg(name).toFloat(), lo, hi);
}

static void apply_u8_arg(const char *name, uint8_t &value, int lo, int hi) {
  if (server.hasArg(name)) value = (uint8_t)clamp_int(server.arg(name).toInt(), lo, hi);
}

static void apply_u16_arg(const char *name, uint16_t &value, int lo, int hi) {
  if (server.hasArg(name)) value = (uint16_t)clamp_int(server.arg(name).toInt(), lo, hi);
}

static uint8_t get_u8_arg(const char *name, uint8_t current, int lo, int hi) {
  if (!server.hasArg(name)) return current;
  return (uint8_t)clamp_int(server.arg(name).toInt(), lo, hi);
}

static uint16_t get_u16_arg(const char *name, uint16_t current, int lo, int hi) {
  if (!server.hasArg(name)) return current;
  return (uint16_t)clamp_int(server.arg(name).toInt(), lo, hi);
}

static void handle_set_tuning() {
  apply_float_arg("volumeGain", cfg.volumeGain, 0.1f, 20.0f);
  apply_float_arg("bassGain", cfg.bassGain, 0.1f, 20.0f);
  apply_float_arg("midGain", cfg.midGain, 0.1f, 20.0f);
  apply_float_arg("trebleGain", cfg.trebleGain, 0.1f, 20.0f);
  apply_float_arg("lrGain", cfg.lrGain, 0.1f, 20.0f);

  apply_float_arg("volumeFloor", cfg.volumeFloor, 0.0f, 2000.0f);
  apply_float_arg("bassFloor", cfg.bassFloor, 0.0f, 2000.0f);
  apply_float_arg("midFloor", cfg.midFloor, 0.0f, 2000.0f);
  apply_float_arg("trebleFloor", cfg.trebleFloor, 0.0f, 2000.0f);

  apply_float_arg("attack", cfg.attack, 0.01f, 1.0f);
  apply_float_arg("release", cfg.release, 0.01f, 1.0f);
  apply_float_arg("peakDecay", cfg.peakDecay, 0.900f, 0.999f);

  apply_u8_arg("beatVolMin", cfg.beatVolMin, 0, 255);
  apply_u8_arg("beatBassMin", cfg.beatBassMin, 0, 255);
  apply_float_arg("beatSensitivity", cfg.beatSensitivity, 0.10f, 5.0f);
  apply_float_arg("beatMinOnset", cfg.beatMinOnset, 0.0f, 2.0f);
  apply_u8_arg("beatTempoAssist", cfg.beatTempoAssist, 0, 1);
  apply_float_arg("beatPredictStrength", cfg.beatPredictStrength, 0.05f, 2.0f);
  apply_u16_arg("beatPredictToleranceMs", cfg.beatPredictToleranceMs, 10, 500);
  apply_u16_arg("beatCooldownMs", cfg.beatCooldownMs, 40, 1000);
  apply_u16_arg("beatHoldMs", cfg.beatHoldMs, 10, 1000);
  apply_u16_arg("beatMinBpm", cfg.beatMinBpm, 30, 220);
  apply_u16_arg("beatMaxBpm", cfg.beatMaxBpm, 60, 300);
  apply_u16_arg("packetIntervalMs", cfg.packetIntervalMs, 15, 500);

  if (server.hasArg("shiftTo16")) cfg.shiftTo16 = clamp_int(server.arg("shiftTo16").toInt(), 12, 20);

  if (server.args() > 0) mark_led_dirty();
  forceEspNowSend = true;
  no_cache();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_set_led() {
  apply_u8_arg("activeStrands", ledCfg.activeStrands, 1, MAX_STRANDS);
  apply_u16_arg("maxLedsPerStrand", ledCfg.maxLedsPerStrand, 1, 1000);
  apply_u8_arg("globalBrightness", ledCfg.globalBrightness, 0, 255);
  apply_u8_arg("globalHue", ledCfg.globalHue, 0, 255);
  apply_u8_arg("colorMap", ledCfg.colorMap, 0, 5);
  apply_u8_arg("redScale", ledCfg.redScale, 0, 255);
  apply_u8_arg("greenScale", ledCfg.greenScale, 0, 255);
  apply_u8_arg("blueScale", ledCfg.blueScale, 0, 255);
  apply_u8_arg("gamma10", ledCfg.gamma10, 10, 30);

  for (int s = 0; s < MAX_STRANDS; s++) {
    char name[24];

    // StrandSettings is packed for ESP-NOW packet compatibility. Do not pass its
    // uint16_t fields by reference; use value-return helpers instead.
    snprintf(name, sizeof(name), "s%d_enabled", s); ledCfg.strand[s].enabled = get_u8_arg(name, ledCfg.strand[s].enabled, 0, 1);
    snprintf(name, sizeof(name), "s%d_pin", s); ledCfg.strand[s].pin = get_u8_arg(name, ledCfg.strand[s].pin, 0, 48);
    snprintf(name, sizeof(name), "s%d_ledType", s); ledCfg.strand[s].ledType = get_u8_arg(name, ledCfg.strand[s].ledType, 0, 6);
    snprintf(name, sizeof(name), "s%d_ledCount", s); ledCfg.strand[s].ledCount = get_u16_arg(name, ledCfg.strand[s].ledCount, 0, 1000);
    snprintf(name, sizeof(name), "s%d_mode", s); ledCfg.strand[s].mode = get_u8_arg(name, ledCfg.strand[s].mode, 0, LED_MODE_MAX);
    snprintf(name, sizeof(name), "s%d_brightness", s); ledCfg.strand[s].brightness = get_u8_arg(name, ledCfg.strand[s].brightness, 0, 255);
    snprintf(name, sizeof(name), "s%d_hue", s); ledCfg.strand[s].hue = get_u8_arg(name, ledCfg.strand[s].hue, 0, 255);
    snprintf(name, sizeof(name), "s%d_saturation", s); ledCfg.strand[s].saturation = get_u8_arg(name, ledCfg.strand[s].saturation, 0, 255);
    snprintf(name, sizeof(name), "s%d_speed", s); ledCfg.strand[s].speed = get_u8_arg(name, ledCfg.strand[s].speed, 0, 255);
    snprintf(name, sizeof(name), "s%d_density", s); ledCfg.strand[s].density = get_u8_arg(name, ledCfg.strand[s].density, 0, 255);
    snprintf(name, sizeof(name), "s%d_sensitivity", s); ledCfg.strand[s].sensitivity = get_u8_arg(name, ledCfg.strand[s].sensitivity, 0, 255);
    snprintf(name, sizeof(name), "s%d_strobe", s); ledCfg.strand[s].strobe = get_u8_arg(name, ledCfg.strand[s].strobe, 0, 255);
    snprintf(name, sizeof(name), "s%d_twinkle", s); ledCfg.strand[s].twinkle = get_u8_arg(name, ledCfg.strand[s].twinkle, 0, 255);
    snprintf(name, sizeof(name), "s%d_direction", s); ledCfg.strand[s].direction = get_u8_arg(name, ledCfg.strand[s].direction, 0, 2);
    // DJ feedback: strobe colour (0 = white, else hue + 1) and colour mood (from == to = none).
    snprintf(name, sizeof(name), "s%d_strobeColor", s); ledCfg.strand[s].strobeColor = get_u8_arg(name, ledCfg.strand[s].strobeColor, 0, 255);
    snprintf(name, sizeof(name), "s%d_moodFrom", s); ledCfg.strand[s].moodFrom = get_u8_arg(name, ledCfg.strand[s].moodFrom, 0, 255);
    snprintf(name, sizeof(name), "s%d_moodTo", s); ledCfg.strand[s].moodTo = get_u8_arg(name, ledCfg.strand[s].moodTo, 0, 255);
  }

  ledCfg.activeStrands = (uint8_t)clamp_int(ledCfg.activeStrands, 1, MAX_STRANDS);
  ledCfg.maxLedsPerStrand = (uint16_t)clamp_int(ledCfg.maxLedsPerStrand, 1, 1000);
  for (int s = 0; s < MAX_STRANDS; s++) {
    if (ledCfg.strand[s].ledCount > ledCfg.maxLedsPerStrand) ledCfg.strand[s].ledCount = ledCfg.maxLedsPerStrand;
  }

  if (server.args() > 0) mark_led_dirty();
  forceEspNowSend = true;
  no_cache();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_save() {
  const bool saved = save_config();
  char out[192];
  snprintf(
    out,
    sizeof(out),
    "{\"saved\":%s,\"activeStrands\":%u,\"redScale\":%u,\"greenScale\":%u,\"blueScale\":%u%s}",
    saved ? "true" : "false",
    ledCfg.activeStrands,
    ledCfg.redScale,
    ledCfg.greenScale,
    ledCfg.blueScale,
    saved ? "" : ",\"error\":\"Preferences read-back verification failed\""
  );
  no_cache();
  server.send(saved ? 200 : 500, "application/json", out);
}

// Default audio tuning and looks. The wiring (active strands, pins, LED counts, types, which strands
// are on) and the colour calibration are kept: with autosave, defaults are saved too, and a lost
// LED count or pin would leave a room dark.
static void handle_defaults() {
  const LedConfig keep = ledCfg;
  set_default_config();
  ledCfg.activeStrands = keep.activeStrands;
  ledCfg.maxLedsPerStrand = keep.maxLedsPerStrand;
  ledCfg.colorMap = keep.colorMap;
  ledCfg.redScale = keep.redScale;
  ledCfg.greenScale = keep.greenScale;
  ledCfg.blueScale = keep.blueScale;
  ledCfg.gamma10 = keep.gamma10;
  for (int s = 0; s < MAX_STRANDS; s++) {
    ledCfg.strand[s].enabled = keep.strand[s].enabled;
    ledCfg.strand[s].pin = keep.strand[s].pin;
    ledCfg.strand[s].ledType = keep.strand[s].ledType;
    ledCfg.strand[s].ledCount = keep.strand[s].ledCount;
  }
  reset_agc();
  reset_beat_state();
  mark_led_dirty();
  forceEspNowSend = true;
  no_cache();
  server.send(200, "application/json", "{\"defaults\":true}");
}

static void handle_reset_agc() {
  reset_agc();
  no_cache();
  server.send(200, "application/json", "{\"reset\":true}");
}

static void handle_reset_beat() {
  reset_beat_state();
  no_cache();
  server.send(200, "application/json", "{\"resetBeat\":true}");
}


static void append_cfg_json(String &s) {
  s += "\"cfg\":{";
  s += "\"shiftTo16\":" + String(cfg.shiftTo16);
  s += ",\"volumeGain\":" + String(cfg.volumeGain, 3);
  s += ",\"bassGain\":" + String(cfg.bassGain, 3);
  s += ",\"midGain\":" + String(cfg.midGain, 3);
  s += ",\"trebleGain\":" + String(cfg.trebleGain, 3);
  s += ",\"lrGain\":" + String(cfg.lrGain, 3);
  s += ",\"volumeFloor\":" + String(cfg.volumeFloor, 2);
  s += ",\"bassFloor\":" + String(cfg.bassFloor, 2);
  s += ",\"midFloor\":" + String(cfg.midFloor, 2);
  s += ",\"trebleFloor\":" + String(cfg.trebleFloor, 2);
  s += ",\"attack\":" + String(cfg.attack, 3);
  s += ",\"release\":" + String(cfg.release, 3);
  s += ",\"peakDecay\":" + String(cfg.peakDecay, 3);
  s += ",\"beatVolMin\":" + String(cfg.beatVolMin);
  s += ",\"beatBassMin\":" + String(cfg.beatBassMin);
  s += ",\"beatSensitivity\":" + String(cfg.beatSensitivity, 3);
  s += ",\"beatMinOnset\":" + String(cfg.beatMinOnset, 3);
  s += ",\"beatTempoAssist\":" + String(cfg.beatTempoAssist);
  s += ",\"beatPredictStrength\":" + String(cfg.beatPredictStrength, 3);
  s += ",\"beatPredictToleranceMs\":" + String(cfg.beatPredictToleranceMs);
  s += ",\"beatCooldownMs\":" + String(cfg.beatCooldownMs);
  s += ",\"beatHoldMs\":" + String(cfg.beatHoldMs);
  s += ",\"beatMinBpm\":" + String(cfg.beatMinBpm);
  s += ",\"beatMaxBpm\":" + String(cfg.beatMaxBpm);
  s += ",\"packetIntervalMs\":" + String(cfg.packetIntervalMs);
  s += "}";
}

static void append_led_json(String &s) {
  s += ",\"led\":{";
  s += "\"activeStrands\":" + String(ledCfg.activeStrands);
  s += ",\"maxLedsPerStrand\":" + String(ledCfg.maxLedsPerStrand);
  s += ",\"globalBrightness\":" + String(ledCfg.globalBrightness);
  s += ",\"globalHue\":" + String(ledCfg.globalHue);
  s += ",\"colorMap\":" + String(ledCfg.colorMap);
  s += ",\"redScale\":" + String(ledCfg.redScale);
  s += ",\"greenScale\":" + String(ledCfg.greenScale);
  s += ",\"blueScale\":" + String(ledCfg.blueScale);
  s += ",\"gamma10\":" + String(ledCfg.gamma10);
  s += ",\"strand\":[";
  for (int i = 0; i < MAX_STRANDS; i++) {
    if (i > 0) s += ",";
    StrandSettings &st = ledCfg.strand[i];
    s += "{";
    s += "\"enabled\":" + String(st.enabled);
    s += ",\"pin\":" + String(st.pin);
    s += ",\"ledType\":" + String(st.ledType);
    s += ",\"ledCount\":" + String(st.ledCount);
    s += ",\"mode\":" + String(st.mode);
    s += ",\"brightness\":" + String(st.brightness);
    s += ",\"hue\":" + String(st.hue);
    s += ",\"saturation\":" + String(st.saturation);
    s += ",\"speed\":" + String(st.speed);
    s += ",\"density\":" + String(st.density);
    s += ",\"sensitivity\":" + String(st.sensitivity);
    s += ",\"strobe\":" + String(st.strobe);
    s += ",\"twinkle\":" + String(st.twinkle);
    s += ",\"direction\":" + String(st.direction);
    s += ",\"strobeColor\":" + String(st.strobeColor);
    s += ",\"moodFrom\":" + String(st.moodFrom);
    s += ",\"moodTo\":" + String(st.moodTo);
    s += "}";
  }
  s += "]}";
}
static void handle_state() {
  uint32_t now = millis();

  String s;
  s.reserve(7500);

  s += "{";
  s += "\"ms\":" + String(now);
  s += ",\"volume\":" + String(last_packet.volume);
  s += ",\"bass\":" + String(last_packet.bass);
  s += ",\"mid\":" + String(last_packet.mid);
  s += ",\"treble\":" + String(last_packet.treble);
  s += ",\"left\":" + String(last_packet.left);
  s += ",\"right\":" + String(last_packet.right);
  s += ",\"beat\":" + String(last_packet.beat);
  s += ",\"beatPulse\":" + String(last_packet.beatPulse);
  s += ",\"beatConfidence\":" + String(last_packet.beatConfidence);
  s += ",\"beatPhase\":" + String(last_packet.beatPhase);
  s += ",\"packetBpm\":" + String(last_packet.beatBpm);
  s += ",\"clip\":" + String(last_packet.clip);

  s += ",\"rawVolume\":" + String(raw_volume, 2);
  s += ",\"rawBass\":" + String(raw_bass, 2);
  s += ",\"rawMid\":" + String(raw_mid, 2);
  s += ",\"rawTreble\":" + String(raw_treble, 2);
  s += ",\"rawLeft\":" + String(raw_left, 2);
  s += ",\"rawRight\":" + String(raw_right, 2);

  s += ",\"beatOnset\":" + String(debugOnset, 4);
  s += ",\"beatThreshold\":" + String(debugThreshold, 4);
  s += ",\"beatScore\":" + String(debugScore, 3);
  s += ",\"spectralFlux\":" + String(debugSpectralFlux, 4);
  s += ",\"lowFlux\":" + String(debugLowFlux, 4);
  s += ",\"highFlux\":" + String(debugHighFlux, 4);
  s += ",\"fftPeakHz\":" + String(debugFftPeakHz, 1);
  s += ",\"fftReady\":" + String(debugFftReady);
  s += ",\"fftSize\":" + String(FFT_SIZE);
  s += ",\"fftBands\":" + String(FFT_BAND_COUNT);
  s += ",\"beatBpm\":" + String(debugBpm, 2);
  s += ",\"beatAge\":" + String(debugBeatAgeMs, 1);
  s += ",\"beatPeriod\":" + String(debugBeatPeriodMs, 1);
  s += ",\"beatCandidate\":" + String(debugCandidate);
  s += ",\"beatTempoAssist\":" + String(debugTempoAssist);

  s += ",\"pps\":" + String(packets_per_sec);
  s += ",\"enqOk\":" + String(send_enqueue_ok);
  s += ",\"enqErr\":" + String(send_enqueue_err);
  s += ",\"busySkip\":" + String(send_skipped_busy);
  s += ",\"cbOk\":" + String((uint32_t)send_cb_ok);
  s += ",\"cbFail\":" + String((uint32_t)send_cb_fail);
  s += ",\"lastErr\":" + String(last_send_err);
  s += ",\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? 1 : 0);
  s += ",\"wifiRssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127);
  s += ",\"wifiIp\":\"" + WiFi.localIP().toString() + "\"";
  s += ",\"wifiChannel\":" + String(active_wifi_channel);
  s += ",\"freeHeap\":" + String(ESP.getFreeHeap());
  s += ",\"minHeap\":" + String(ESP.getMinFreeHeap());
  s += ",\"fftUs\":" + String(lastFftUs);
  s += ",\"fftAvgUs\":" + String(avgFftUs);
  s += ",\"analyzeUs\":" + String(lastAnalyzeUs);
  s += ",\"httpUs\":" + String(lastHttpUs);
  s += ",\"loopUs\":" + String(lastLoopUs);

  s += ",";
  append_cfg_json(s);
  append_led_json(s);


  s += "}";

  no_cache();
  server.send(200, "application/json", s);
}

static void handle_live() {
  uint32_t startedUs = micros();
  uint32_t now = millis();

  // Fixed buffer avoids repeated heap allocation/fragmentation on the
  // frequently-polled endpoint.
  char out[2100];
  int n = snprintf(
    out, sizeof(out),
    "{\"ms\":%lu,\"volume\":%u,\"bass\":%u,\"mid\":%u,\"treble\":%u,\"left\":%u,\"right\":%u,"
    "\"beat\":%u,\"beatPulse\":%u,\"beatConfidence\":%u,\"beatPhase\":%u,\"packetBpm\":%u,\"clip\":%u,"
    "\"rawVolume\":%.2f,\"rawBass\":%.2f,\"rawMid\":%.2f,\"rawTreble\":%.2f,"
    "\"beatOnset\":%.4f,\"beatThreshold\":%.4f,\"beatScore\":%.3f,"
    "\"spectralFlux\":%.4f,\"lowFlux\":%.4f,\"highFlux\":%.4f,"
    "\"fftPeakHz\":%.1f,\"fftReady\":%u,\"fftSize\":%u,"
    "\"beatBpm\":%.2f,\"beatAge\":%.1f,\"beatCandidate\":%u,\"beatTempoAssist\":%u,"
    "\"pps\":%lu,\"enqOk\":%lu,\"enqErr\":%lu,\"busySkip\":%lu,"
    "\"cbOk\":%lu,\"cbFail\":%lu,\"lastErr\":%lu,"
    "\"wifiConnected\":%u,\"wifiRssi\":%d,\"wifiIp\":\"%s\",\"wifiChannel\":%u,\"freeHeap\":%u,\"minHeap\":%u,"
    "\"fftUs\":%lu,\"fftAvgUs\":%lu,\"analyzeUs\":%lu,\"httpUs\":%lu,\"loopUs\":%lu}",
    (unsigned long)now,
    last_packet.volume, last_packet.bass, last_packet.mid, last_packet.treble,
    last_packet.left, last_packet.right,
    last_packet.beat, last_packet.beatPulse,
    last_packet.beatConfidence, last_packet.beatPhase, last_packet.beatBpm,
    last_packet.clip,
    raw_volume, raw_bass, raw_mid, raw_treble,
    debugOnset, debugThreshold, debugScore,
    debugSpectralFlux, debugLowFlux, debugHighFlux,
    debugFftPeakHz, debugFftReady, FFT_SIZE,
    debugBpm, debugBeatAgeMs, debugCandidate, debugTempoAssist,
    (unsigned long)packets_per_sec,
    (unsigned long)send_enqueue_ok,
    (unsigned long)send_enqueue_err,
    (unsigned long)send_skipped_busy,
    (unsigned long)send_cb_ok,
    (unsigned long)send_cb_fail,
    (unsigned long)last_send_err,
    (unsigned int)(WiFi.status() == WL_CONNECTED ? 1 : 0),
    WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127,
    WiFi.localIP().toString().c_str(), active_wifi_channel,
    ESP.getFreeHeap(), ESP.getMinFreeHeap(),
    (unsigned long)lastFftUs,
    (unsigned long)avgFftUs,
    (unsigned long)lastAnalyzeUs,
    (unsigned long)lastHttpUs,
    (unsigned long)lastLoopUs
  );

  if (n < 0 || (size_t)n >= sizeof(out)) {
    no_cache();
    server.send(500, "application/json", "{\"error\":\"live_json_overflow\"}");
    return;
  }

  lastLiveBytes = (uint32_t)n;
  no_cache();
  server.send(200, "application/json", out);
  lastHttpUs = micros() - startedUs;
  if (lastHttpUs > maxHttpUs) maxHttpUs = lastHttpUs;
  httpRequestCount++;
}

static void handle_health() {
  uint32_t startedUs = micros();
  char out[256];
  String ipString = WiFi.localIP().toString();
  int n = snprintf(
    out, sizeof(out),
    "{\"ok\":%u,\"mode\":\"station\",\"uptimeMs\":%lu,\"ip\":\"%s\",\"rssi\":%d,\"channel\":%u,\"heap\":%lu,\"pps\":%lu}",
    WiFi.status() == WL_CONNECTED ? 1U : 0U,
    (unsigned long)millis(),
    ipString.c_str(),
    WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127,
    active_wifi_channel,
    (unsigned long)ESP.getFreeHeap(),
    (unsigned long)packets_per_sec
  );
  if (n < 0) n = 0;
  no_cache();
  server.send(200, "application/json", out);
  lastHttpUs = micros() - startedUs;
  if (lastHttpUs > maxHttpUs) maxHttpUs = lastHttpUs;
  httpRequestCount++;
}

// Art-Net receive diagnostics (Phase 10). ?n=<channels> (default 12, max 512); ?reset=1 zeroes
// the interval stats (maxIntervalMs, stalls) before reporting.
static void handle_artnet() {
  if (server.hasArg("reset")) reset_artnet_interval_stats();
  int n = server.hasArg("n") ? server.arg("n").toInt() : 12;
  String s;
  s.reserve(4096);
  append_artnet_json(s, n);
  no_cache();
  server.send(200, "application/json", s);
}

// Control source and safety (Phases 13-14). Saved: source, fallback, standalone, strobeHz, cap.
// Live only: strobe (off at every boot), address.
//   ?source=auto|artnet|hybrid|audio|standalone &fallback=audio|standalone &standalone=0..255
//   &strobe=0|1 &strobeHz=1..15 &cap=8..255 (rig brightness cap, every source; 255 = none) &address=1..497
//   &strobeSense=0..255 (strobe sensitivity: 255 = always, lower = flashes only in loud parts; saved)
//   &cellGap=0..12 (spaced SoundSwitch cells: dark gap in 16ths of a cell, 0 = none; saved)
//   &statusLed=0|1 (the S3's onboard RGB status light; saved, dark by default)
//   &kickFlash=0..255 (rig-wide white flash on bass hits, saved; 0 = off)
//   &cells=0..150 (SoundSwitch fixture: 0 = LUMINA-16, N = N plain RGB cells; saved)
//   &fixture=lumina16|<layout id> (named layouts, see /api/fixtures; saved)
//   &fx=0|1 &fxHit=0..7 (off/ripple/glitter/pump/shatter/comet/duck/ping-pong) &beam=0..255 &spread=1..8
//   &fxBeam=0|1 &fxSparkle=0|1 &fxStrobe=0|1 &fxAlways=0|1 (Lumina FX fixture looks; all saved)
//   &split=0|1 (cell layouts: split the cells across the active strands; saved)
//   &update=1|2 (put that room's receiver into wireless update mode for 10 minutes)
//   (?artnet=1|0 = source artnet|audio)
static void handle_control() {
  bool persist = false;
  router::Source parsed;
  if (server.hasArg("source") && router::parse(server.arg("source").c_str(), parsed)) {
    control.source = parsed;
    persist = true;
  }
  if (server.hasArg("artnet")) {  // Phase 12 alias
    control.source = server.arg("artnet").toInt() != 0 ? router::Source::Artnet : router::Source::Audio;
    persist = true;
  }
  if (server.hasArg("fallback") && router::parse(server.arg("fallback").c_str(), parsed)) {
    control.autoFallback = router::sanitize_fallback(parsed);
    persist = true;
  }
  if (server.hasArg("standalone")) {
    control.standaloneLevel = (uint8_t)clamp_int(server.arg("standalone").toInt(), 0, 255);
    persist = true;
  }
  if (server.hasArg("strobeHz")) {
    control.strobeHz = strobe_clamp_hz((uint8_t)clamp_int(server.arg("strobeHz").toInt(), 1, STROBE_MAX_HZ));
    persist = true;
  }
  if (server.hasArg("strobe")) control.strobeEnabled = server.arg("strobe").toInt() != 0;
  if (server.hasArg("strobeSense")) {  // 255 = always; lower = only the loud parts (saved)
    control.strobeSense = (uint8_t)clamp_int(server.arg("strobeSense").toInt(), 0, 255);
    persist = true;
  }
  if (server.hasArg("cellGap")) {  // spaced SoundSwitch cells: gap in 16ths of a cell, 0 = none (saved)
    control.cellGap = (uint8_t)clamp_int(server.arg("cellGap").toInt(), 0, CELL_GAP_MAX);
    persist = true;
  }
  if (server.hasArg("statusLed")) {  // the S3's onboard RGB status light (saved; dark by default)
    control.statusLed = server.arg("statusLed").toInt() != 0;
    status_rgb_set_enabled(control.statusLed);
    persist = true;
  }
  if (server.hasArg("address")) control.address = (uint16_t)clamp_int(server.arg("address").toInt(), 1, 512 - 15);
  if (server.hasArg("fixture")) {  // "lumina16", or a named layout id 1..N (/api/fixtures)
    const String f = server.arg("fixture");
    if (f == "lumina16") {
      control.fixture = 0;
      persist = true;
    } else if (layout::preset((uint8_t)clamp_int(f.toInt(), 0, 255)) != nullptr) {
      control.fixture = layout::preset_code((uint8_t)f.toInt());
      persist = true;
    }
  }
  if (server.hasArg("cells")) {  // 0 = LUMINA-16, 1..150 = N plain RGB cells
    control.fixture = (uint8_t)clamp_int(server.arg("cells").toInt(), 0, MAX_CELLS);
    persist = true;
  }
  if (server.hasArg("fx")) { control.fxEnabled = server.arg("fx").toInt() != 0; persist = true; }
  if (server.hasArg("fxHit")) { control.fxHit = (uint8_t)clamp_int(server.arg("fxHit").toInt(), 0, 7); persist = true; }
  if (server.hasArg("fxBeam")) { control.beamOn = server.arg("fxBeam").toInt() != 0; persist = true; }
  if (server.hasArg("fxSparkle")) { control.sparkleOn = server.arg("fxSparkle").toInt() != 0; persist = true; }
  if (server.hasArg("fxStrobe")) { control.fxStrobeOn = server.arg("fxStrobe").toInt() != 0; persist = true; }
  if (server.hasArg("update")) request_room_update((uint8_t)clamp_int(server.arg("update").toInt(), 0, 2));  // not saved
  if (server.hasArg("split")) { control.splitStrands = server.arg("split").toInt() != 0; persist = true; }
  if (server.hasArg("fxAlways")) { control.kicksAlways = server.arg("fxAlways").toInt() != 0; persist = true; }
  if (server.hasArg("beam")) { control.beamStrength = (uint8_t)clamp_int(server.arg("beam").toInt(), 0, 255); persist = true; }
  if (server.hasArg("spread")) { control.beamSpread = (uint8_t)clamp_int(server.arg("spread").toInt(), 1, 8); persist = true; }
  if (server.hasArg("kickFlash")) {
    control.kickFlash = (uint8_t)clamp_int(server.arg("kickFlash").toInt(), 0, 255);
    persist = true;
  }
  if (server.hasArg("cap")) {
    control.brightnessCap =
        (uint8_t)clamp_int(server.arg("cap").toInt(), RIG_BRIGHTNESS_CAP_MIN, RIG_BRIGHTNESS_CAP_MAX);
    persist = true;
  }
  if (persist && !save_control_settings()) Serial.println("WARNING: control settings could not be saved");
  String s;
  s.reserve(768);
  append_control_json(s, millis());
  no_cache();
  server.send(200, "application/json", s);
}

// Page preferences, saved on the S3 so every phone and PC shows the same (Phase 15, extended for
// the DJs 2026-10-31). Bit n of uiHiddenModes = LED mode n; bit k of uiHiddenKicks = kick look k.
// The Live page shows uiLiveModes (0 = empty slot) and the colour moods in uiLiveMoods (bit i =
// mood i of the page's list); uiArchive shows the retired genres on the Advanced page.
static uint64_t uiHiddenModes = 0;
static uint8_t uiHiddenKicks = 0;
static constexpr uint8_t kLiveSlots = 6;
static uint8_t uiLiveModes[kLiveSlots] = {25, 28, 46, 47, 49, 51};  // Hyper Strobe, DnB Split, Hyper Scatter/Grid, Split Mirror/Strobe
static uint16_t uiLiveMoods = 0xFFFF;
static uint8_t uiArchive = 0;

static void load_ui_prefs() {
  Preferences store;
  if (!store.begin("lumina_ui", true)) return;
  uiHiddenModes = store.getULong64("modes", 0);
  uiHiddenKicks = store.getUChar("kicks", 0);
  // isKey first: getBytesLength logs an error for a key that was never saved (a fresh board).
  if (store.isKey("live") && store.getBytesLength("live") == kLiveSlots) store.getBytes("live", uiLiveModes, kLiveSlots);
  uiLiveMoods = store.getUShort("moods", 0xFFFF);
  uiArchive = store.getUChar("archive", 0);
  store.end();
  for (uint8_t i = 0; i < kLiveSlots; i++) if (uiLiveModes[i] > LED_MODE_MAX) uiLiveModes[i] = 0;
}

//   /api/ui[?hideModes=<decimal bit mask>&hideKicks=0..255&liveModes=m1,..,m6&liveMoods=<mask>&archive=0|1]
static void handle_ui() {
  bool changed = false;
  if (server.hasArg("hideModes")) {
    uiHiddenModes = strtoull(server.arg("hideModes").c_str(), nullptr, 10) & ((1ULL << (LED_MODE_MAX + 1)) - 1);
    changed = true;
  }
  if (server.hasArg("hideKicks")) {
    uiHiddenKicks = (uint8_t)clamp_int(server.arg("hideKicks").toInt(), 0, 255);
    changed = true;
  }
  if (server.hasArg("liveModes")) {
    const String v = server.arg("liveModes");
    int from = 0;
    for (uint8_t i = 0; i < kLiveSlots; i++) {
      const int comma = v.indexOf(',', from);
      const String part = comma < 0 ? v.substring(from) : v.substring(from, comma);
      uiLiveModes[i] = part.length() ? (uint8_t)clamp_int(part.toInt(), 0, LED_MODE_MAX) : 0;
      from = comma < 0 ? v.length() : comma + 1;
    }
    changed = true;
  }
  if (server.hasArg("liveMoods")) {
    uiLiveMoods = (uint16_t)clamp_int(server.arg("liveMoods").toInt(), 0, 0xFFFF);
    changed = true;
  }
  if (server.hasArg("archive")) {
    uiArchive = server.arg("archive").toInt() != 0 ? 1 : 0;
    changed = true;
  }
  if (changed) {
    Preferences store;
    if (store.begin("lumina_ui", false)) {
      store.putULong64("modes", uiHiddenModes);
      store.putUChar("kicks", uiHiddenKicks);
      store.putBytes("live", uiLiveModes, kLiveSlots);
      store.putUShort("moods", uiLiveMoods);
      store.putUChar("archive", uiArchive);
      store.end();
    }
  }
  char out[192];
  snprintf(out, sizeof(out), "{\"hideModes\":%llu,\"hideKicks\":%u,\"liveModes\":[%u,%u,%u,%u,%u,%u],\"liveMoods\":%u,\"archive\":%u}",
           (unsigned long long)uiHiddenModes, uiHiddenKicks, uiLiveModes[0], uiLiveModes[1], uiLiveModes[2], uiLiveModes[3],
           uiLiveModes[4], uiLiveModes[5], uiLiveMoods, uiArchive);
  no_cache();
  server.send(200, "application/json", out);
}

// Per-mode slider memory (DJ feedback, 2026-10-31): the Feel sliders a DJ last used for each mode
// (speed, density, audio response, decay, sparkle), so switching modes and back keeps them.
// A mode nobody has touched has no entry; the page then uses its tuned starting values.
static constexpr uint8_t kFeelFields = 5;
static uint8_t feelValues[LED_MODE_MAX + 1][kFeelFields];
static uint64_t feelTouched = 0;  // bit n = mode n has remembered values

static void load_feel() {
  Preferences store;
  if (!store.begin("lumina_ui", true)) return;
  if (store.isKey("feel") && store.getBytesLength("feel") == sizeof(feelValues)) {
    store.getBytes("feel", feelValues, sizeof(feelValues));
    feelTouched = store.getULong64("feelmask", 0) & ((1ULL << (LED_MODE_MAX + 1)) - 1);
  }
  store.end();
}

static void save_feel() {
  Preferences store;
  if (!store.begin("lumina_ui", false)) return;
  store.putBytes("feel", feelValues, sizeof(feelValues));
  store.putULong64("feelmask", feelTouched);
  store.end();
}

//   /api/feel                         -> {"modes":{"25":[speed,density,sensitivity,decay,sparkle],...}}
//   /api/feel?mode=N&v=a,b,c,d,e      remember mode N's sliders (saved a few seconds later)
//   /api/feel?forget=N|all            back to the tuned starting values
static void handle_feel() {
  if (server.hasArg("mode") && server.hasArg("v")) {
    const int mode = server.arg("mode").toInt();
    if (mode >= 0 && mode <= LED_MODE_MAX) {
      const String v = server.arg("v");
      int from = 0;
      for (uint8_t i = 0; i < kFeelFields; i++) {
        const int comma = v.indexOf(',', from);
        const String part = comma < 0 ? v.substring(from) : v.substring(from, comma);
        feelValues[mode][i] = (uint8_t)clamp_int(part.toInt(), 0, 255);
        from = comma < 0 ? v.length() : comma + 1;
      }
      feelTouched |= 1ULL << mode;
      feelDirtyMs = millis() | 1U;
    }
  }
  if (server.hasArg("forget")) {
    const String f = server.arg("forget");
    if (f == "all") feelTouched = 0;
    else if (f.toInt() >= 0 && f.toInt() <= LED_MODE_MAX) feelTouched &= ~(1ULL << f.toInt());
    feelDirtyMs = millis() | 1U;
  }
  String s;
  s.reserve(1024);
  s += "{\"modes\":{";
  bool firstEntry = true;
  for (uint8_t m = 0; m <= LED_MODE_MAX; m++) {
    if (!(feelTouched & (1ULL << m))) continue;
    if (!firstEntry) s += ",";
    firstEntry = false;
    s += "\"" + String(m) + "\":[";
    for (uint8_t i = 0; i < kFeelFields; i++) {
      if (i) s += ",";
      s += String(feelValues[m][i]);
    }
    s += "]";
  }
  s += "}}";
  no_cache();
  server.send(200, "application/json", s);
}

void web_autosave(uint32_t nowMs) {
  if (ledDirtyMs && nowMs - ledDirtyMs >= kAutosaveMs) {
    ledDirtyMs = 0;
    Serial.println(save_config() ? "Autosaved settings" : "WARNING: autosave failed (Preferences read-back)");
  }
  if (feelDirtyMs && nowMs - feelDirtyMs >= kAutosaveMs) {
    feelDirtyMs = 0;
    save_feel();
  }
}

void setup_web_server() {
  load_ui_prefs();
  load_feel();
  server.on("/", handle_root);
  server.on("/api/state", handle_state);
  server.on("/api/live", handle_live);
  server.on("/health", handle_health);
  server.on("/api/artnet", handle_artnet);
  server.on("/api/control", handle_control);
  server.on("/api/fixtures", []() {  // Phase 15: named SoundSwitch layouts for the Setup page
    String s;
    s.reserve(4096);
    append_fixture_list_json(s);
    no_cache();
    server.send(200, "application/json", s);
  });
  server.on("/api/ui", handle_ui);
  server.on("/api/feel", handle_feel);
  server.on("/api/setTuning", handle_set_tuning);
  server.on("/api/setLed", handle_set_led);
  server.on("/api/save", handle_save);
  server.on("/api/defaults", handle_defaults);
  server.on("/api/resetAgc", handle_reset_agc);
  server.on("/api/resetBeat", handle_reset_beat);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started");
}
