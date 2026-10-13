#include "LuminaWebServer.h"
#include <WiFi.h>
#include "IndexHtml.h"
#include "../config/ConfigManager.h"
#include "../lighting/LightingState.h"
#include "../audio/AudioAnalyzer.h"
#include "../network/WiFiManager.h"
#include "../network/EspNowSender.h"
#include "../util/Clamp.h"

WebServer server(80);

uint32_t lastLoopUs = 0;
uint32_t maxLoopUs = 0;
uint32_t lastHttpUs = 0;
uint32_t maxHttpUs = 0;
uint32_t httpRequestCount = 0;
uint32_t lastLiveBytes = 0;

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
    snprintf(name, sizeof(name), "s%d_mode", s); ledCfg.strand[s].mode = get_u8_arg(name, ledCfg.strand[s].mode, 0, 31);
    snprintf(name, sizeof(name), "s%d_brightness", s); ledCfg.strand[s].brightness = get_u8_arg(name, ledCfg.strand[s].brightness, 0, 255);
    snprintf(name, sizeof(name), "s%d_hue", s); ledCfg.strand[s].hue = get_u8_arg(name, ledCfg.strand[s].hue, 0, 255);
    snprintf(name, sizeof(name), "s%d_saturation", s); ledCfg.strand[s].saturation = get_u8_arg(name, ledCfg.strand[s].saturation, 0, 255);
    snprintf(name, sizeof(name), "s%d_speed", s); ledCfg.strand[s].speed = get_u8_arg(name, ledCfg.strand[s].speed, 0, 255);
    snprintf(name, sizeof(name), "s%d_density", s); ledCfg.strand[s].density = get_u8_arg(name, ledCfg.strand[s].density, 0, 255);
    snprintf(name, sizeof(name), "s%d_sensitivity", s); ledCfg.strand[s].sensitivity = get_u8_arg(name, ledCfg.strand[s].sensitivity, 0, 255);
    snprintf(name, sizeof(name), "s%d_strobe", s); ledCfg.strand[s].strobe = get_u8_arg(name, ledCfg.strand[s].strobe, 0, 255);
    snprintf(name, sizeof(name), "s%d_twinkle", s); ledCfg.strand[s].twinkle = get_u8_arg(name, ledCfg.strand[s].twinkle, 0, 255);
    snprintf(name, sizeof(name), "s%d_direction", s); ledCfg.strand[s].direction = get_u8_arg(name, ledCfg.strand[s].direction, 0, 2);
    ledCfg.strand[s].reserved8 = 0;
  }

  ledCfg.activeStrands = (uint8_t)clamp_int(ledCfg.activeStrands, 1, MAX_STRANDS);
  ledCfg.maxLedsPerStrand = (uint16_t)clamp_int(ledCfg.maxLedsPerStrand, 1, 1000);
  for (int s = 0; s < MAX_STRANDS; s++) {
    if (ledCfg.strand[s].ledCount > ledCfg.maxLedsPerStrand) ledCfg.strand[s].ledCount = ledCfg.maxLedsPerStrand;
  }

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

static void handle_defaults() {
  set_default_config();
  reset_agc();
  reset_beat_state();
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

void setup_web_server() {
  server.on("/", handle_root);
  server.on("/api/state", handle_state);
  server.on("/api/live", handle_live);
  server.on("/health", handle_health);
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
