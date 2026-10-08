// Lumina v22 home-Wi-Fi sender: rave/electronic effects, VU tools in Audio Tune, beat detector disabled, no recorder
#include <Arduino.h>
#include <ESP_I2S.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "soc/soc_caps.h"
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <math.h>
#include <stddef.h>

// Arduino IDE automatically generates function prototypes for .ino files.
// Some Arduino-ESP32 versions place those generated prototypes before custom
// packed structs, so forward-declare the packet type before any functions.
struct AudioLedPacket;
struct PersistedConfigBlob;

I2SClass I2S;
WebServer server(80);
Preferences prefs;

// ============================================================
// PCM1808 wiring
// ============================================================
static constexpr int PIN_MCLK = 20; // PCM1808 SCK / SCKI / MCLK
static constexpr int PIN_BCLK = 21; // PCM1808 BCK
static constexpr int PIN_LRCK = 47; // PCM1808 LRC / LRCK
static constexpr int PIN_DIN  = 48; // PCM1808 OUT / DOUT -> ESP32-S3

static constexpr uint32_t SAMPLE_RATE = 48000;
static constexpr int BLOCK_FRAMES = 256;

// ============================================================
// Home Wi-Fi station mode for the web UI
// ============================================================
// The sender joins your normal 2.4 GHz Wi-Fi, so the phone/laptop keeps
// internet access while controlling Lumina. For the lowest latency, keep the
// control device and this ESP32 on the same SSID/VLAN/subnet.
static const char *WIFI_SSID = "WIFISSID";
static const char *WIFI_PASS = "WIFIPASSWORD";
static const char *WIFI_HOSTNAME = "lumina-sender";

// The address below matches the LAN address used in your earlier tests.
// Reserve 192.168.30.99 in pfSense/DHCP or change these values before upload.
static constexpr bool USE_STATIC_IP = true;
IPAddress local_ip(192, 168, 30, 99);
IPAddress gateway(192, 168, 30, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(192, 168, 30, 1);   //local resolver
IPAddress dns2(192, 168, 50, 50); // Pi-hole
// Arduino-ESP32 WiFi.config() accepts two DNS servers. Configure 1.1.1.1
// as an upstream/fallback inside pfSense or Pi-hole if desired. Do not pass
// a third argument here because that overload does not exist.

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;
static uint8_t active_wifi_channel = 1;
static bool wifiWasConnected = false;
static uint32_t lastWifiReconnectAttemptMs = 0;

// Onboard RGB status LED:
//   dim blue blink = connecting
//   green           = joined to home Wi-Fi
//   red blink       = disconnected/reconnecting
// The code automatically disables this LED if its physical GPIO conflicts
// with one of the proven PCM1808 pins. Many S3 boards place the RGB LED on
// GPIO48; this project currently uses GPIO48 as PCM1808 DOUT/DIN.
static constexpr uint8_t STATUS_RGB_BRIGHTNESS = 32;
static bool statusRgbUsable = false;

// ESP-NOW broadcast. Your LED receiver must be on the same Wi-Fi channel
// as the router channel printed by this sender.
static const uint8_t BROADCAST_ADDR[6] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ============================================================
// Onboard RGB status helpers
// ============================================================
static int onboard_rgb_physical_pin() {
#if defined(RGB_BUILTIN)
  int pin = (int)RGB_BUILTIN;
  #if defined(SOC_GPIO_PIN_COUNT)
    if (pin >= SOC_GPIO_PIN_COUNT) pin -= SOC_GPIO_PIN_COUNT;
  #endif
  return pin;
#else
  return -1;
#endif
}

static bool pin_conflicts_with_pcm1808(int pin) {
  return pin == PIN_MCLK || pin == PIN_BCLK || pin == PIN_LRCK || pin == PIN_DIN;
}

static void status_rgb_write(uint8_t r, uint8_t g, uint8_t b) {
#if defined(RGB_BUILTIN)
  if (statusRgbUsable) rgbLedWrite(RGB_BUILTIN, r, g, b);
#else
  (void)r; (void)g; (void)b;
#endif
}

static void setup_status_rgb() {
#if defined(RGB_BUILTIN)
  int physicalPin = onboard_rgb_physical_pin();
  Serial.print("Onboard RGB physical GPIO: ");
  Serial.println(physicalPin);

  if (pin_conflicts_with_pcm1808(physicalPin)) {
    statusRgbUsable = false;
    Serial.println("WARNING: onboard RGB status LED disabled because its GPIO conflicts with PCM1808 wiring.");
    Serial.println("If this is GPIO48, move PCM1808 DOUT to another free GPIO before enabling the onboard RGB LED.");
    return;
  }

  statusRgbUsable = true;
  status_rgb_write(0, 0, 0);
#else
  Serial.println("No RGB_BUILTIN definition for the selected Arduino board; onboard RGB status is disabled.");
#endif
}

static void show_connecting_rgb(uint32_t nowMs) {
  if (!statusRgbUsable) return;
  bool on = ((nowMs / 300U) & 1U) == 0;
  status_rgb_write(0, 0, on ? STATUS_RGB_BRIGHTNESS : 0);
}

static void show_connected_rgb() {
  status_rgb_write(0, STATUS_RGB_BRIGHTNESS, 0);
}

static void show_disconnected_rgb(uint32_t nowMs) {
  if (!statusRgbUsable) return;
  bool on = ((nowMs / 350U) & 1U) == 0;
  status_rgb_write(on ? STATUS_RGB_BRIGHTNESS : 0, 0, 0);
}

// ============================================================
// ESP-NOW packet format
// Keep these structs exactly the same on the LED receiver.
// ============================================================
static constexpr uint16_t PACKET_MAGIC = 0xA17D;
static constexpr uint8_t PACKET_VERSION = 12;
static constexpr uint8_t CONFIG_SCHEMA_VERSION = 19;
static constexpr uint8_t MAX_STRANDS = 4;

// Default LED receiver GPIO assignments. The sender does not electrically
// drive these pins; it sends them to the receiver in the ESP-NOW packet.
// The Hardware tab can still override them and save the new values.
static constexpr uint8_t DEFAULT_LED_PINS[MAX_STRANDS] = {33, 25, 26, 27};
static constexpr uint16_t DEFAULT_LED_COUNTS[MAX_STRANDS] = {10, 0, 0, 0};

enum LedType : uint8_t {
  LED_TYPE_WS2815_WS2812_RGB_800 = 0,
  LED_TYPE_WS2811_RGB_800 = 1,
  LED_TYPE_WS2811_GRB_800 = 2,
  LED_TYPE_WS2811_RGB_400 = 3,
  LED_TYPE_SK6812_RGB_GRB = 4,
  LED_TYPE_SK6812_RGBW_GRBW = 5,
  LED_TYPE_SK6812_RGBW_RGBW = 6
};

// Logical RGB -> native strip Color(r,g,b) argument order.
// This is for color calibration without touching LED wiring.
enum ColorMap : uint8_t {
  COLOR_MAP_RGB = 0,
  COLOR_MAP_RBG = 1,
  COLOR_MAP_GRB = 2,
  COLOR_MAP_GBR = 3,
  COLOR_MAP_BRG = 4,
  COLOR_MAP_BGR = 5
};

struct __attribute__((packed)) StrandSettings {
  uint8_t enabled;      // 0/1
  uint8_t mode;         // LED pattern number
  uint8_t brightness;   // strand brightness 0-255
  uint8_t hue;          // base hue 0-255
  uint8_t saturation;   // saturation 0-255
  uint8_t speed;        // animation speed 0-255
  uint8_t density;      // twinkle/fill/density 0-255
  uint8_t sensitivity;  // how much audio affects this strand 0-255
  uint8_t strobe;       // strobe strength/decay 0-255
  uint8_t twinkle;      // twinkle/glitter amount 0-255
  uint8_t direction;    // 0 forward, 1 reverse, 2 mirror/center
  uint8_t pin;          // LED receiver GPIO pin, assigned from browser
  uint8_t ledType;      // browser-selected LED chipset/color order
  uint8_t reserved8;    // keeps following uint16_t on an even byte boundary
  uint16_t ledCount;    // active LED count for this strand, assigned from browser
  uint16_t reserved;    // future use
};

struct __attribute__((packed)) AudioLedPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t size;

  uint16_t sequence;
  uint32_t ms;

  uint8_t volume;
  uint8_t bass;
  uint8_t mid;
  uint8_t treble;
  uint8_t left;
  uint8_t right;
  uint8_t beat;       // 255 during beat hold, else 0
  uint8_t clip;
  uint8_t beatPulse;  // increments once for every detected beat
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands; // how many strand slots the receiver should use, 1-4
  uint8_t flags;         // bit0=onset candidate, bit1=tempo-assisted candidate
  uint16_t maxLedsPerStrand; // browser-set safety/config cap

  // LED color calibration. The receiver applies this before writing pixels.
  uint8_t colorMap;      // 0 RGB, 1 RBG, 2 GRB, 3 GBR, 4 BRG, 5 BGR
  uint8_t redScale;      // 0-255 logical red correction
  uint8_t greenScale;    // 0-255 logical green correction
  uint8_t blueScale;     // 0-255 logical blue correction
  uint8_t gamma10;       // 10=1.0, 22=2.2, etc.

  // Beat diagnostics used by the receiver's Beat Tracker LED mode.
  // These replace three previously-unused reserved bytes, so packet layout
  // remains compact while PACKET_VERSION distinguishes the new semantics.
  uint8_t beatConfidence; // 0-255, adaptive onset score
  uint8_t beatPhase;      // 0-255, phase from one accepted beat to the next
  uint8_t beatBpm;        // integer BPM, 0 when tempo is not locked

  StrandSettings strand[MAX_STRANDS];
  uint16_t reserved;
};

static_assert(sizeof(AudioLedPacket) < 250, "AudioLedPacket must fit in an ESP-NOW v1 payload");

// ============================================================
// Tunable audio/beat settings
// ============================================================
struct TuningConfig {
  float volumeGain;
  float bassGain;
  float midGain;
  float trebleGain;
  float lrGain;

  float volumeFloor;
  float bassFloor;
  float midFloor;
  float trebleFloor;

  float attack;
  float release;
  float peakDecay;

  uint8_t beatVolMin;
  uint8_t beatBassMin;

  // Modern onset-style beat detector settings.
  // Lower beatSensitivity = more beats. Higher = fewer false beats.
  float beatSensitivity;

  // Absolute onset floor. Raise it if silence/noise causes fake beats.
  float beatMinOnset;

  // Tempo assist lets weaker onsets trigger when they are near the predicted beat time.
  uint8_t beatTempoAssist;
  float beatPredictStrength;
  uint16_t beatPredictToleranceMs;

  uint16_t beatCooldownMs;
  uint16_t beatHoldMs;
  uint16_t beatMinBpm;
  uint16_t beatMaxBpm;

  uint16_t packetIntervalMs;
  int shiftTo16;
};

struct LedConfig {
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands;
  uint16_t maxLedsPerStrand;
  uint8_t colorMap;
  uint8_t redScale;
  uint8_t greenScale;
  uint8_t blueScale;
  uint8_t gamma10;
  StrandSettings strand[MAX_STRANDS];
};

// Compact, versioned, CRC-protected configuration storage. Two alternating
// slots are used so a reset or power loss during a save leaves the previous
// valid configuration intact.
static constexpr uint32_t PERSIST_MAGIC = 0x4C554D32UL; // ASCII-ish "LUM2"
static constexpr uint16_t PERSIST_VERSION = 1;
static constexpr char PERSIST_NAMESPACE[] = "lumina_cfg";
static constexpr char PERSIST_SLOT_A[] = "cfgA";
static constexpr char PERSIST_SLOT_B[] = "cfgB";
static constexpr char LEGACY_NAMESPACE[] = "audioLedV13";
static constexpr const char *LEGACY_NAMESPACES[] = {
  "audioLedV13",
  "audioLedV12",
  "audioLedV9",
  "audioLedV8",
  "audioLedV7",
  "audioLedV6",
  "audioLedV4",
  "audioTune3",
  "audioTune"
};

struct PersistedConfigBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  TuningConfig tuning;
  LedConfig leds;
  uint32_t crc32;
};

TuningConfig cfg;
LedConfig ledCfg;
static uint32_t persistedConfigSequence = 0;

// Explicit prototypes for functions that use AudioLedPacket. These stop the
// Arduino .ino preprocessor from inventing invalid prototypes before the
// packed packet struct is declared.
static void update_beat_detector(AudioLedPacket &p, uint32_t now, bool runFft);
static AudioLedPacket analyze_audio_block(int frames);
static void maybe_send_espnow(AudioLedPacket &p);

// ============================================================
// Audio / analyzer state
// ============================================================
int32_t raw_samples[BLOCK_FRAMES * 2];
int16_t beat_mono_samples[BLOCK_FRAMES];

uint16_t sequence_num = 0;
uint8_t beat_pulse_counter = 0;
AudioLedPacket last_packet = {};

float peak_volume = 1200.0f;
float peak_bass = 300.0f;
float peak_mid = 300.0f;
float peak_treble = 300.0f;
float peak_left = 1200.0f;
float peak_right = 1200.0f;

float smooth_volume = 0.0f;
float smooth_bass = 0.0f;
float smooth_mid = 0.0f;
float smooth_treble = 0.0f;
float smooth_left = 0.0f;
float smooth_right = 0.0f;

float raw_volume = 0.0f;
float raw_bass = 0.0f;
float raw_mid = 0.0f;
float raw_treble = 0.0f;
float raw_left = 0.0f;
float raw_right = 0.0f;

// Simple crossover-ish filter states.
float lp40 = 0.0f;
float lp160 = 0.0f;
float lp250 = 0.0f;
float lp4000 = 0.0f;

static constexpr float PI_F = 3.14159265359f;
const float A40   = 1.0f - expf(-2.0f * PI_F * 40.0f   / SAMPLE_RATE);
const float A160  = 1.0f - expf(-2.0f * PI_F * 160.0f  / SAMPLE_RATE);
const float A250  = 1.0f - expf(-2.0f * PI_F * 250.0f  / SAMPLE_RATE);
const float A4000 = 1.0f - expf(-2.0f * PI_F * 4000.0f / SAMPLE_RATE);

// ============================================================
// End-goal FFT spectral-flux beat detector state
//
// Uses a true 1024-point Hann-windowed FFT. Audio is read in 256-frame
// blocks so the web server gets CPU time about every 5.33 ms. The FFT runs
// every two blocks, producing a 512-sample hop (50% overlap), a 10.67 ms
// onset update interval, and about 93.75 FFT updates/second.
//   48 kHz / 1024 = 46.875 Hz bin spacing
//   512-sample hop = 10.67 ms update rate
//
// Detection chain:
//   1. mono audio -> 1024-sample rolling window
//   2. Hann window -> in-place radix-2 FFT
//   3. log-spaced band magnitudes
//   4. half-wave rectified spectral flux
//   5. adaptive threshold + local peak picking
//   6. tempo-assisted recovery for weak-but-on-time kicks
// ============================================================
static constexpr int FFT_SIZE = 1024;
static constexpr int FFT_UPDATE_EVERY_BLOCKS = 2;
static constexpr int FFT_HOP = BLOCK_FRAMES * FFT_UPDATE_EVERY_BLOCKS;
static constexpr int FFT_BIN_COUNT = (FFT_SIZE / 2) + 1;
static constexpr int FFT_BAND_COUNT = 36;
static constexpr float FFT_MIN_HZ = 43.0f;
static constexpr float FFT_MAX_HZ = 12000.0f;

// v22 effects react to band energy directly; beat detection is intentionally disabled.
static constexpr bool ENABLE_BEAT_DETECTOR = false;

float fftWindow[FFT_SIZE] = {};
float fftReal[FFT_SIZE] = {};
float fftImag[FFT_SIZE] = {};
int16_t fftMonoWindow[FFT_SIZE] = {};
uint16_t fftFilledSamples = 0;
bool fftReady = false;
bool fftTablesReady = false;

uint16_t fftBandStart[FFT_BAND_COUNT] = {};
uint16_t fftBandEnd[FFT_BAND_COUNT] = {};
float fftBandCenterHz[FFT_BAND_COUNT] = {};

// Current and previous log-scaled FFT band magnitudes used by the
// spectral-flux beat detector. These were missing in the previous v7 file.
float fftBandLog[FFT_BAND_COUNT] = {};
float prevBandLog[FFT_BAND_COUNT] = {};
bool fftBandPrimed = false;

float prevLogVolume = 0.0f;
bool beatStatePrimed = false;
uint16_t beatWarmupBlocks = 0;

float onsetMean = 0.0f;
float onsetDev = 0.020f;

float onsetPrev2 = 0.0f;
float onsetPrev1 = 0.0f;
float thresholdPrev2 = 0.0f;
float thresholdPrev1 = 0.0f;
uint32_t onsetMsPrev2 = 0;
uint32_t onsetMsPrev1 = 0;

uint32_t lastBeatMs = 0;
uint32_t beatHoldUntilMs = 0;
float beatPeriodMs = 0.0f;

float debugOnset = 0.0f;
float debugThreshold = 0.0f;
float debugScore = 0.0f;
float debugBpm = 0.0f;
float debugBeatAgeMs = 0.0f;
float debugBeatPeriodMs = 0.0f;
uint8_t debugCandidate = 0;
uint8_t debugTempoAssist = 0;
float debugSpectralFlux = 0.0f;
float debugLowFlux = 0.0f;
float debugHighFlux = 0.0f;
float debugFftPeakHz = 0.0f;
uint8_t debugFftReady = 0;

// FFT and loop scheduling/performance diagnostics.
uint8_t fftBlockCounter = 0;
uint32_t fftRunCount = 0;
uint32_t lastFftUs = 0;
uint32_t maxFftUs = 0;
uint32_t avgFftUs = 0;
uint32_t lastAnalyzeUs = 0;
uint32_t maxAnalyzeUs = 0;
uint32_t lastLoopUs = 0;
uint32_t maxLoopUs = 0;
uint32_t lastHttpUs = 0;
uint32_t maxHttpUs = 0;
uint32_t httpRequestCount = 0;
uint32_t lastLiveBytes = 0;

// Set when a browser command should be pushed on the very next audio block
// instead of waiting for the normal packet interval.
volatile bool forceEspNowSend = false;

// ============================================================
// ESP-NOW stats
// ============================================================
volatile bool espnow_busy = false;
volatile uint32_t send_cb_ok = 0;
volatile uint32_t send_cb_fail = 0;

uint32_t send_enqueue_ok = 0;
uint32_t send_enqueue_err = 0;
uint32_t send_skipped_busy = 0;
uint32_t last_send_err = 0;
uint32_t last_send_ms = 0;
uint32_t busy_since_ms = 0;

uint32_t last_pps_ms = 0;
uint32_t last_pps_count = 0;
uint32_t packets_per_sec = 0;

// One-time migration keeps audio/effect tuning but moves legacy saved
// hardware settings to the proven v16 pin order and RGB LED protocol.
bool configMigrationNeeded = false;

// ============================================================
// Web page
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>Lumina LED Controller V22</title>
  <style>
    :root {
      --bg:#0d1016;
      --card:#191e27;
      --card2:#202634;
      --line:#313949;
      --text:#f5f7fb;
      --muted:#aab3c2;
      --blue:#2d74ff;
      --green:#00d084;
      --yellow:#ffde59;
      --red:#ff4d4d;
    }
    * { box-sizing:border-box; }
    body {
      font-family: Arial, Helvetica, sans-serif;
      background:var(--bg);
      color:var(--text);
      margin:0;
      padding:12px;
      line-height:1.35;
    }
    .page { max-width:1180px; margin:0 auto; }
    h1 { margin:4px 0 4px 0; font-size:26px; }
    h2 { margin:0 0 10px 0; font-size:20px; }
    h3 { margin:0 0 8px 0; font-size:18px; }
    .small { color:var(--muted); font-size:13px; }
    .warn { color:var(--yellow); }
    .tabs {
      position:sticky;
      top:0;
      z-index:5;
      display:flex;
      gap:8px;
      overflow-x:auto;
      overscroll-behavior-x:contain;
      scrollbar-width:thin;
      background:rgba(13,16,22,0.97);
      padding:10px 0;
      backdrop-filter:blur(6px);
      -webkit-overflow-scrolling:touch;
    }
    .tabbtn {
      flex:0 0 auto;
      min-width:108px;
      background:#252b36;
      border:1px solid #3b4352;
      color:#ddd;
      border-radius:12px;
      padding:13px 10px;
      font-size:15px;
      font-weight:700;
      min-height:48px;
      white-space:nowrap;
    }
    .tabbtn.active { background:var(--blue); color:white; border-color:var(--blue); }
    .tabbtn.inactive-strand { opacity:0.58; }
    .tab { display:none; }
    .tab.active { display:block; }
    .card, .strand {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:16px;
      padding:14px;
      margin-bottom:14px;
      box-shadow:0 2px 14px rgba(0,0,0,0.18);
    }
    .hero {
      background:linear-gradient(135deg,#1e2635,#131722);
      border-color:#3d4b63;
    }
    .grid {
      display:grid;
      grid-template-columns:165px minmax(0,1fr) 78px;
      gap:8px;
      align-items:center;
    }
    .grid2 { display:grid; grid-template-columns:repeat(auto-fit,minmax(310px,1fr)); gap:14px; }
    input[type=range] { width:100%; accent-color:var(--blue); }
    input, select {
      width:100%;
      background:#0b0d12;
      color:var(--text);
      border:1px solid #3b4352;
      border-radius:9px;
      padding:9px;
      font-size:16px;
    }
    button {
      background:var(--blue);
      color:white;
      border:0;
      border-radius:11px;
      padding:12px 14px;
      margin:4px 4px 4px 0;
      font-size:15px;
      font-weight:700;
      min-height:44px;
      touch-action:manipulation;
    }
    button.secondary { background:#3b4352; }
    button.danger { background:#a33; }
    .meterrow { display:grid; grid-template-columns:75px 1fr 50px; gap:8px; align-items:center; margin:7px 0; }
    .meter { height:22px; background:#07080a; border-radius:999px; overflow:hidden; border:1px solid #313742; }
    .bar { height:100%; width:0%; background:linear-gradient(90deg,#2d74ff,#00e676,#ffd54f,#ff5252); transition:width 70ms linear; }
    .beat { display:inline-block; width:24px; height:24px; border-radius:50%; background:#313744; vertical-align:middle; margin-left:8px; border:1px solid #566070; }
    .beat.on { background:#ffeb3b; box-shadow:0 0 18px #ffeb3b; border-color:#fff08a; }
    .beat.candidate.on { background:#00e5ff; box-shadow:0 0 18px #00e5ff; border-color:#8df6ff; }
    .beatline { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin:8px 0; }
    .beatpill { background:#0b0d12; border:1px solid #3b4352; border-radius:999px; padding:8px 12px; }
    pre { background:#080a0f; color:#d9f7ff; padding:12px; border-radius:10px; overflow-x:auto; white-space:pre-wrap; }
    .stat { display:inline-block; margin-right:14px; margin-bottom:5px; }
    .pill { display:inline-block; background:#0b0d12; border:1px solid #3b4352; border-radius:999px; padding:6px 10px; margin:2px; }
    .choice-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(118px,1fr)); gap:8px; margin:8px 0 12px 0; }
    .choice-btn {
      width:100%;
      min-height:58px;
      background:#242b38;
      color:#e8edf7;
      border:1px solid #3b4352;
      border-radius:14px;
      padding:10px 8px;
      margin:0;
      text-align:center;
      font-size:14px;
    }
    .choice-btn.active { background:var(--blue); border-color:#8ab2ff; color:white; box-shadow:0 0 0 2px rgba(45,116,255,0.22); }
    .color-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(76px,1fr)); gap:8px; margin:8px 0 12px 0; }
    .color-btn {
      min-height:54px;
      border-radius:14px;
      border:2px solid #3b4352;
      color:white;
      text-shadow:0 1px 4px rgba(0,0,0,0.9);
      font-weight:800;
      padding:8px 4px;
      margin:0;
    }
    .color-btn.active { border-color:white; box-shadow:0 0 0 3px rgba(255,255,255,0.18); }
    .section-title { margin-top:12px; margin-bottom:4px; color:#dce7ff; font-weight:800; }
    .mode-group-title { margin:14px 0 7px; color:#a9c4ff; font-size:13px; font-weight:900; letter-spacing:.08em; text-transform:uppercase; }
    .mode-group-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(118px,1fr)); gap:8px; margin:0 0 12px; }
    .mode-note { background:#101722; border-left:3px solid #2d74ff; border-radius:8px; padding:9px 10px; margin:8px 0 12px; color:#cbd8ef; font-size:13px; }
    .compact-controls { margin-top:8px; }
    .front-actions { display:flex; flex-wrap:wrap; gap:8px; }
    .front-actions button { flex:1 1 160px; }
    .toggle-row { display:grid; grid-template-columns:1fr 110px; gap:8px; align-items:center; }
    .hidden-input { display:none; }
    .status-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(145px,1fr)); gap:8px; }
    .status-box { background:#0b0d12; border:1px solid #3b4352; border-radius:10px; padding:9px; }
    .status-box b { display:block; font-size:19px; margin-top:2px; }
    .strand-heading { display:flex; align-items:center; justify-content:space-between; gap:10px; flex-wrap:wrap; }
    .strand-heading h2 { margin:0; }
    .toast {
      position:fixed;
      left:50%;
      bottom:18px;
      transform:translate(-50%,20px);
      z-index:30;
      max-width:calc(100vw - 24px);
      background:#111722;
      color:#fff;
      border:1px solid #42506a;
      border-radius:12px;
      padding:11px 16px;
      box-shadow:0 8px 28px rgba(0,0,0,.45);
      opacity:0;
      pointer-events:none;
      transition:opacity .18s ease, transform .18s ease;
      text-align:center;
      font-weight:700;
    }
    .toast.show { opacity:1; transform:translate(-50%,0); }
    .toast.error { background:#541f28; border-color:#c74d61; }
    .save-note { color:var(--green); font-weight:700; }
    @media (max-width:760px) {
      body { padding:10px; }
      h1 { font-size:22px; }
      .tabs { margin-left:-2px; margin-right:-2px; gap:7px; }
      .tabbtn { min-width:102px; min-height:52px; font-size:14px; padding:10px 9px; }
      .card, .strand { padding:12px; border-radius:14px; }
      .grid { grid-template-columns:1fr; gap:5px; }
      .grid > span { text-align:right; color:#dce7ff; margin-bottom:7px; }
      .grid2 { grid-template-columns:1fr; }
      .choice-grid { grid-template-columns:repeat(2,1fr); }
      .color-grid { grid-template-columns:repeat(3,1fr); }
      .meterrow { grid-template-columns:66px 1fr 44px; }
      button { width:100%; margin-right:0; }
      .front-actions { display:block; }
      .front-actions button { width:100%; }
    }
  </style>
</head>
<body>
<div class="page">
  <h1>Lumina LED Controller V22</h1>
  <div class="small">Home Wi-Fi mode. Your phone or laptop keeps internet access while controlling Lumina. For best response, keep the controller and ESP32 on the same SSID/VLAN.</div>

  <div class="tabs" aria-label="Lumina control pages">
    <button class="tabbtn active" id="btn_all" onclick="showTab('all')">All LEDs</button>
    <button class="tabbtn" id="btn_strand0" onclick="showTab('strand0')">Strand 1</button>
    <button class="tabbtn" id="btn_strand1" onclick="showTab('strand1')">Strand 2</button>
    <button class="tabbtn" id="btn_strand2" onclick="showTab('strand2')">Strand 3</button>
    <button class="tabbtn" id="btn_strand3" onclick="showTab('strand3')">Strand 4</button>
    <button class="tabbtn" id="btn_hardware" onclick="showTab('hardware')">Hardware</button>
    <button class="tabbtn" id="btn_tuning" onclick="showTab('tuning')">Audio Tune</button>
  </div>

  <div id="tab_all" class="tab active">
    <div class="card hero">
      <h2>Control All LEDs</h2>
      <p class="small">The simple phone-friendly page. Changes apply to every active strand. Use a Strand tab for individual looks, Hardware for setup/calibration, and Audio Tune for music response.</p>
      <div class="front-actions">
        <button onclick="sendLedNow(true)">Apply All LEDs</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>

    <div class="card">
      <h2>Master Controls</h2>
      <div class="grid">
        <label>Global brightness</label><input id="globalBrightness" type="range" min="0" max="255" step="1"><span id="globalBrightnessVal"></span>
        <label>Global hue shift</label><input id="globalHue" type="range" min="0" max="255" step="1"><span id="globalHueVal"></span>
      </div>
      <div class="section-title">Quick global hue shift</div>
      <div class="color-grid" id="globalColorButtons"></div>
    </div>

    <div class="card">
      <h2>All Active Strands</h2>
      <p class="small">These controls copy one mode and color to every strand selected by Active Strands on the Hardware page.</p>
      <div class="section-title">Mode</div>
      <div id="allModeButtons"></div>
      <div class="section-title">Color</div>
      <div class="color-grid" id="allColorButtons"></div>
      <div class="compact-controls grid">
        <label>Brightness</label><input id="all_brightness" type="range" min="0" max="255" step="1"><span id="all_brightnessVal"></span>
        <label>Speed</label><input id="all_speed" type="range" min="0" max="255" step="1"><span id="all_speedVal"></span>
        <label>Density</label><input id="all_density" type="range" min="0" max="255" step="1"><span id="all_densityVal"></span>
        <label>Audio sensitivity</label><input id="all_sensitivity" type="range" min="0" max="255" step="1"><span id="all_sensitivityVal"></span>
        <label>Strobe/decay</label><input id="all_strobe" type="range" min="0" max="255" step="1"><span id="all_strobeVal"></span>
        <label>Twinkle/glitter</label><input id="all_twinkle" type="range" min="0" max="255" step="1"><span id="all_twinkleVal"></span>
        <label>Direction</label><input id="all_direction" type="range" min="0" max="2" step="1"><span id="all_directionVal"></span>
      </div>
      <div class="front-actions">
        <button onclick="copyStrandOneToAll()">Copy Strand 1 to All</button>
        <button onclick="sendLedNow(true)">Apply All</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>
  </div>

  <div id="tab_strand0" class="tab"><div id="strandControl_0"></div></div>
  <div id="tab_strand1" class="tab"><div id="strandControl_1"></div></div>
  <div id="tab_strand2" class="tab"><div id="strandControl_2"></div></div>
  <div id="tab_strand3" class="tab"><div id="strandControl_3"></div></div>

  <div id="tab_hardware" class="tab">
    <div class="card">
      <h2>LED Hardware Setup</h2>
      <div class="grid">
        <label>Active strands</label><input id="activeStrands" type="range" min="1" max="4" step="1"><span id="activeStrandsVal"></span>
        <label>Max LEDs/strand</label><input id="maxLedsPerStrand" type="number" min="1" max="1000" step="1"><span id="maxLedsPerStrandVal"></span>
      </div>
      <p class="small warn">Changing pins, LED count, or LED type causes the receiver to reconfigure. Save Configuration writes the complete current UI state to flash.</p>
      <div class="front-actions">
        <button onclick="sendLedNow()">Apply Hardware</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>
    <div class="card">
      <h2>Color Calibration</h2>
      <p class="small">Use this only when the color buttons do not match the real strip. The test buttons temporarily put every active strand in Solid Color mode so Red, Green, Blue, and White can be verified directly.</p>
      <input id="colorMap" type="hidden" value="0"><span id="colorMapVal" style="display:none"></span>
      <div class="section-title">Channel map</div>
      <div class="mode-grid color-map-grid" id="colorMapButtons"></div>
      <div class="grid">
        <label>Red scale</label><input id="redScale" type="range" min="0" max="255" step="1"><span id="redScaleVal"></span>
        <label>Green scale</label><input id="greenScale" type="range" min="0" max="255" step="1"><span id="greenScaleVal"></span>
        <label>Blue scale</label><input id="blueScale" type="range" min="0" max="255" step="1"><span id="blueScaleVal"></span>
        <label>Gamma x10</label><input id="gamma10" type="range" min="10" max="30" step="1"><span id="gamma10Val"></span>
      </div>
      <div class="front-actions">
        <button onclick="setCalibrationTest(0)">Test Red</button>
        <button onclick="setCalibrationTest(96)">Test Green</button>
        <button onclick="setCalibrationTest(165)">Test Blue</button>
        <button onclick="setCalibrationTest(0,0)">Test White</button>
        <button class="secondary" onclick="resetCalibration()">Reset Calibration</button>
        <button onclick="saveAll()">Save Calibration</button>
      </div>
    </div>

    <div class="card">
      <h2>ESP-NOW Status</h2>
      <div class="status-grid">
        <div class="status-box">Packets/sec <b id="pps">0</b></div>
        <div class="status-box">Queued OK <b id="enqOk">0</b></div>
        <div class="status-box">Queue errors <b id="enqErr">0</b></div>
        <div class="status-box">Busy skips <b id="busySkip">0</b></div>
        <div class="status-box">Callback OK <b id="cbOk">0</b></div>
        <div class="status-box">Callback fail <b id="cbFail">0</b></div>
        <div class="status-box">Last ESP err <b id="lastErr">0</b></div>
        <div class="status-box">Wi-Fi RSSI <b id="wifiRssi">0</b></div>
        <div class="status-box">Local IP <b id="wifiIp">0.0.0.0</b></div>
        <div class="status-box">Wi-Fi channel <b id="wifiChannel">0</b></div>
        <div class="status-box">Free heap <b id="freeHeap">0</b></div>
        <div class="status-box">FFT time us <b id="fftUs">0</b></div>
        <div class="status-box">Analyze time us <b id="analyzeUs">0</b></div>
        <div class="status-box">HTTP time us <b id="httpUs">0</b></div>
        <div class="status-box">Loop time us <b id="loopUs">0</b></div>
      </div>
      <p class="small">Use <code>/health</code> for Uptime Kuma. It is much lighter than polling <code>/api/state</code>.</p>
    </div>

    <div class="grid2" id="hardwareStrandControls"></div>
  </div>

  <div id="tab_tuning" class="tab">
    <div class="card hero">
      <h2>Input Audio Tuning</h2>
      <div id="meters_tuning"></div>
      <p class="small" id="rawText"></p>
    </div>

    <div class="card">
      <h2>Audio Calibration & VU Preview</h2>
      <p class="small">VU meters live here as tuning tools and are intentionally hidden from the normal lighting-mode list. Use the clean baseline, tune silence floors first, then compare overall, bass, mid, and treble response.</p>
      <div class="front-actions">
        <button onclick="applyCleanAudioBaseline()">Load Clean Audio Baseline</button>
        <button onclick="setVuPreview(6,'Overall VU')">Overall VU on Strand 1</button>
        <button onclick="setVuPreview(11,'Bass VU')">Bass VU on Strand 1</button>
        <button onclick="setVuPreview(12,'Mid VU')">Mid VU on Strand 1</button>
        <button onclick="setVuPreview(13,'Treble VU')">Treble VU on Strand 1</button>
        <button onclick="setAllOverallVu()">Overall VU on All Strands</button>
        <button onclick="setFourMeterLayout()">4-Strand VU Layout</button>
        <button class="secondary" onclick="restoreSolidColor()">Restore Solid Color</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>

    <div class="card">
      <h2>Level tuning</h2>
      <div class="grid">
        <label>Shift to 16</label><input id="shiftTo16" type="range" min="13" max="18" step="1"><span id="shiftTo16Val"></span>
        <label>Volume gain</label><input id="volumeGain" type="range" min="0.1" max="8" step="0.05"><span id="volumeGainVal"></span>
        <label>Bass gain</label><input id="bassGain" type="range" min="0.1" max="12" step="0.05"><span id="bassGainVal"></span>
        <label>Mid gain</label><input id="midGain" type="range" min="0.1" max="12" step="0.05"><span id="midGainVal"></span>
        <label>Treble gain</label><input id="trebleGain" type="range" min="0.1" max="12" step="0.05"><span id="trebleGainVal"></span>
        <label>L/R gain</label><input id="lrGain" type="range" min="0.1" max="8" step="0.05"><span id="lrGainVal"></span>
        <label>Volume floor</label><input id="volumeFloor" type="range" min="0" max="500" step="1"><span id="volumeFloorVal"></span>
        <label>Bass floor</label><input id="bassFloor" type="range" min="0" max="500" step="1"><span id="bassFloorVal"></span>
        <label>Mid floor</label><input id="midFloor" type="range" min="0" max="500" step="1"><span id="midFloorVal"></span>
        <label>Treble floor</label><input id="trebleFloor" type="range" min="0" max="500" step="1"><span id="trebleFloorVal"></span>
      </div>
    </div>

    <div class="card">
      <h2>AGC / smoothing</h2>
      <div class="grid">
        <label>Attack</label><input id="attack" type="range" min="0.02" max="1" step="0.01"><span id="attackVal"></span>
        <label>Release</label><input id="release" type="range" min="0.01" max="0.8" step="0.01"><span id="releaseVal"></span>
        <label>Peak decay</label><input id="peakDecay" type="range" min="0.900" max="0.999" step="0.001"><span id="peakDecayVal"></span>
      </div>
    </div>

    <div class="card" style="display:none">
      <h2>Beat detection (disabled in v22)</h2>
      <div class="beatline">
        <span class="beatpill">Final beat <span id="beatLightFinal" class="beat"></span></span>
        <span class="beatpill">Onset candidate <span id="beatLightCandidate" class="beat candidate"></span></span>
      </div>
      <div class="grid">
        <label>Beat volume min</label><input id="beatVolMin" type="range" min="0" max="255" step="1"><span id="beatVolMinVal"></span>
        <label>Beat bass min</label><input id="beatBassMin" type="range" min="0" max="255" step="1"><span id="beatBassMinVal"></span>
        <label>Beat sensitivity</label><input id="beatSensitivity" type="range" min="0.30" max="3.00" step="0.01"><span id="beatSensitivityVal"></span>
        <label>Beat min onset</label><input id="beatMinOnset" type="range" min="0.000" max="0.300" step="0.001"><span id="beatMinOnsetVal"></span>
        <label>Tempo assist</label><input id="beatTempoAssist" type="range" min="0" max="1" step="1"><span id="beatTempoAssistVal"></span>
        <label>Predict strength</label><input id="beatPredictStrength" type="range" min="0.20" max="1.20" step="0.01"><span id="beatPredictStrengthVal"></span>
        <label>Predict tolerance ms</label><input id="beatPredictToleranceMs" type="range" min="20" max="220" step="5"><span id="beatPredictToleranceMsVal"></span>
        <label>Cooldown ms</label><input id="beatCooldownMs" type="range" min="60" max="500" step="5"><span id="beatCooldownMsVal"></span>
        <label>Hold ms</label><input id="beatHoldMs" type="range" min="20" max="300" step="5"><span id="beatHoldMsVal"></span>
        <label>Min BPM</label><input id="beatMinBpm" type="range" min="40" max="150" step="1"><span id="beatMinBpmVal"></span>
        <label>Max BPM</label><input id="beatMaxBpm" type="range" min="100" max="240" step="1"><span id="beatMaxBpmVal"></span>
        <label>Packet interval ms</label><input id="packetIntervalMs" type="range" min="20" max="120" step="1"><span id="packetIntervalMsVal"></span>
      </div>
      <p class="small" id="beatDebugText"></p>
    </div>

    <div class="card">
      <h2>Actions</h2>
      <button onclick="resetAgc()">Reset AGC</button>
      <button onclick="saveAll()">Save Configuration</button>
      <button class="secondary" onclick="copyAudioSettings()">Copy Audio Settings</button>
      <button class="danger" onclick="loadDefaults()">Defaults</button>
    </div>

    <div class="card">
      <h2>Audio values to write down</h2>
      <pre id="settingsText"></pre>
    </div>
  </div>

</div>
<div id="toast" class="toast" role="status" aria-live="polite"></div>

<script>
const meterNames = ["volume", "bass", "mid", "treble", "left", "right"];
const tuningIds = [
  "shiftTo16",
  "volumeGain", "bassGain", "midGain", "trebleGain", "lrGain",
  "volumeFloor", "bassFloor", "midFloor", "trebleFloor",
  "attack", "release", "peakDecay",
  "beatVolMin", "beatBassMin", "beatSensitivity", "beatMinOnset",
  "beatTempoAssist", "beatPredictStrength", "beatPredictToleranceMs",
  "beatCooldownMs", "beatHoldMs", "beatMinBpm", "beatMaxBpm",
  "packetIntervalMs"
];
const ledModes = [
  [9,  "Solid Color",       "general"],
  [15, "Aurora Drift",      "general"],
  [16, "Starfield",         "general"],
  [17, "Neon Scanner",      "general"],
  [0,  "Off",               "general"],

  [18, "Laser Sweep",       "rave"],
  [19, "Rave Chase",        "rave"],
  [20, "Bass Cannons",      "rave"],
  [21, "DnB Chopper",       "rave"],
  [22, "Acid Chase",        "rave"],
  [23, "Portal Pulse",      "rave"],
  [24, "Spectrum Sparks",   "rave"],
  [25, "Hyper Strobe",      "rave"],
  [26, "Cyber Tunnel",      "rave"],
  [27, "White Lightning",   "rave"],
  [28, "DnB Split",         "rave"],
  [29, "Rave Tiles",        "rave"],
  [30, "Pulse Train",       "rave"],
  [31, "Void Bloom",        "rave"]
];

// Internal diagnostic modes remain available from Audio Tune, but do not
// clutter the normal lighting controls. Legacy v21 IDs remain understood.
const diagnosticModes = [
  [1,"Space Pulse"],[2,"Beat Strobe"],[3,"Bass Comet"],[4,"Twinkle"],
  [5,"Spectrum"],[6,"Overall VU"],[7,"Glitter Kick"],[8,"Void Scanner"],
  [10,"Debug Chase"],[11,"Bass VU"],[12,"Mid VU"],[13,"Treble VU"],
  [14,"Beat Tracker"]
];
const allModeDefinitions = ledModes.concat(diagnosticModes);

const modeDefaults = {
  0:  {speed:0,   density:0,   sensitivity:0,   strobe:0,   twinkle:0},
  9:  {speed:0,   density:0,   sensitivity:0,   strobe:0,   twinkle:0},
  15: {speed:48,  density:118, sensitivity:0,   strobe:175, twinkle:24},
  16: {speed:62,  density:48,  sensitivity:0,   strobe:205, twinkle:170},
  17: {speed:118, density:76,  sensitivity:0,   strobe:185, twinkle:8},
  18: {speed:168, density:82,  sensitivity:0,   strobe:210, twinkle:0},
  19: {speed:178, density:112, sensitivity:0,   strobe:150, twinkle:20},
  20: {speed:118, density:105, sensitivity:168, strobe:175, twinkle:0},
  21: {speed:220, density:142, sensitivity:180, strobe:205, twinkle:16},
  22: {speed:194, density:78,  sensitivity:148, strobe:175, twinkle:38},
  23: {speed:130, density:108, sensitivity:158, strobe:185, twinkle:0},
  24: {speed:154, density:125, sensitivity:175, strobe:180, twinkle:190},
  25: {speed:232, density:74,  sensitivity:185, strobe:225, twinkle:0},
  26: {speed:168, density:98,  sensitivity:138, strobe:185, twinkle:0},
  27: {speed:178, density:82,  sensitivity:185, strobe:230, twinkle:12},
  28: {speed:198, density:124, sensitivity:168, strobe:185, twinkle:16},
  29: {speed:184, density:150, sensitivity:155, strobe:165, twinkle:0},
  30: {speed:205, density:88,  sensitivity:158, strobe:180, twinkle:0},
  31: {speed:120, density:82,  sensitivity:165, strobe:195, twinkle:92}
};

const colorPresets = [
  ["Red", 0, 255, "#ff2b2b"],
  ["Orange", 22, 255, "#ff8c1a"],
  ["Yellow", 42, 255, "#ffd83d"],
  ["Green", 96, 255, "#00d464"],
  ["Cyan", 128, 255, "#00d9ff"],
  ["Blue", 165, 255, "#2d74ff"],
  ["Purple", 190, 255, "#9d4dff"],
  ["Pink", 222, 255, "#ff4db8"],
  ["White", 0, 0, "#ffffff"]
];
const ledTypes = [
  [0, "WS2815 / WS2812-style RGB (RGB, 800kHz) — proven setup"],
  [1, "WS2811 RGB (RGB, 800kHz)"],
  [2, "WS2811 RGB (GRB, 800kHz)"],
  [3, "WS2811 older strips (RGB, 400kHz)"],
  [4, "SK6812 RGB (GRB, 800kHz)"],
  [5, "SK6812 RGBW (GRBW, 800kHz)"],
  [6, "SK6812 RGBW (RGBW, 800kHz)"]
];
const colorMaps = [
  [0, "RGB"],
  [1, "RBG"],
  [2, "GRB"],
  [3, "GBR"],
  [4, "BRG"],
  [5, "BGR"]
];
const MAX_STRANDS = 4;
const modeFields = ["enabled","mode","brightness","hue","saturation","speed","density","sensitivity","strobe","twinkle","direction"];
const allControlFields = ["brightness","speed","density","sensitivity","strobe","twinkle","direction"];
const hardwareFields = ["pin","ledCount","ledType"];
const strandFields = modeFields.concat(hardwareFields);
let loadedOnce = false;
let debounceTune = null;
let debounceLed = null;
let activeTab = "all";
let livePollTimer = null;
let livePollInFlight = false;
let tuningRequestInFlight = false;
let ledRequestInFlight = false;
let forceFullLedRequest = false;
let lastUiBeatPulse = null;
let uiBeatTimer = null;
const pendingTuningIds = new Set();
const pendingLedIds = new Set();

const tabNames = ["all","strand0","strand1","strand2","strand3","hardware","tuning"];

function showTab(name) {
  if (!tabNames.includes(name)) name = "all";
  activeTab = name;
  for (const tab of tabNames) {
    const page = document.getElementById("tab_" + tab);
    const button = document.getElementById("btn_" + tab);
    if (page) page.classList.toggle("active", tab === name);
    if (button) button.classList.toggle("active", tab === name);
  }
  const activeButton = document.getElementById("btn_" + name);
  if (activeButton) activeButton.scrollIntoView({behavior:"smooth", block:"nearest", inline:"center"});
  scheduleLivePoll(0);
}

function makeMeters(containerId) {
  const root = document.getElementById(containerId);
  if (!root) return;
  root.innerHTML = "";
  for (const name of meterNames) {
    root.innerHTML += `<div class="meterrow"><div>${name}</div><div class="meter"><div class="bar" id="${containerId}_${name}Bar"></div></div><div id="${containerId}_${name}Num">0</div></div>`;
  }
}

function setMeter(containerId, name, value) {
  const bar = document.getElementById(containerId + "_" + name + "Bar");
  const num = document.getElementById(containerId + "_" + name + "Num");
  if (!bar || !num) return;
  value = Math.max(0, Math.min(255, value || 0));
  bar.style.width = (value / 255 * 100) + "%";
  num.textContent = value;
}

function setControl(id, value) {
  const el = document.getElementById(id);
  const label = document.getElementById(id + "Val");
  if (!el) return;
  el.value = value;
  if (label) label.textContent = value;
}

function readControl(id) {
  const el = document.getElementById(id);
  return el ? el.value : "0";
}

function modeName(mode) {
  const found = allModeDefinitions.find(m => String(m[0]) === String(mode));
  return found ? found[1] : String(mode);
}

function modeGroupMarkup(prefix, clickFn) {
  const group = (name, title, note) => {
    const buttons = ledModes.filter(m => m[2] === name).map(m =>
      `<button type="button" class="choice-btn" id="${prefix}ModeBtn_${m[0]}" onclick="${clickFn}(${m[0]})">${m[1]}</button>`
    ).join("");
    return `<div class="mode-group-title">${title}</div><div class="mode-note">${note}</div><div class="mode-group-grid">${buttons}</div>`;
  };
  return group("general", "General / No Audio Required", "Solid Color and three dependable ambient effects are first. These modes work even with silence or the audio cable disconnected.") +
         group("rave", "Rave / EDM / Drum & Bass", "Sparse beams, chases, tunnels, bursts, and audio-energy effects designed to leave intentional black space between illuminated sections.");
}

function applyModeDefaultsToStrand(s, mode) {
  const d = modeDefaults[mode];
  const changed = [`s${s}_mode`];
  if (!d) return changed;
  for (const [field, value] of Object.entries(d)) {
    setControl(`s${s}_${field}`, value);
    changed.push(`s${s}_${field}`);
  }
  return changed;
}

function makeGlobalColorButtons() {
  const root = document.getElementById("globalColorButtons");
  root.innerHTML = "";
  for (const c of colorPresets) {
    if (c[0] === "White") continue;
    root.innerHTML += `<button type="button" class="color-btn" style="background:${c[3]};" onclick="setGlobalColor(${c[1]})">${c[0]}</button>`;
  }
}


function makeAllModeButtons() {
  const root = document.getElementById("allModeButtons");
  if (!root) return;
  root.innerHTML = modeGroupMarkup("all", "selectAllMode");
}

function makeAllColorButtons() {
  const root = document.getElementById("allColorButtons");
  if (!root) return;
  root.innerHTML = "";
  for (const c of colorPresets) {
    root.innerHTML += `<button type="button" class="color-btn" id="allColorBtn_${c[0]}" style="background:${c[3]}; color:${c[0] === 'White' ? '#111' : '#fff'}; text-shadow:${c[0] === 'White' ? 'none' : '0 1px 4px rgba(0,0,0,0.9)'};" onclick="setAllColor(${c[1]},${c[2]})">${c[0]}</button>`;
  }
}

function activeStrandCount() {
  return Math.max(1, Math.min(MAX_STRANDS, parseInt(readControl("activeStrands")) || 1));
}

function makeColorMapButtons() {
  const root = document.getElementById("colorMapButtons");
  if (!root) return;
  root.innerHTML = "";
  for (const m of colorMaps) {
    root.innerHTML += `<button type="button" class="choice-btn" id="colorMapBtn_${m[0]}" onclick="setColorMap(${m[0]})">${m[1]}</button>`;
  }
}

function refreshColorMapButtons() {
  const active = parseInt(readControl("colorMap")) || 0;
  for (const m of colorMaps) {
    const el = document.getElementById(`colorMapBtn_${m[0]}`);
    if (el) el.classList.toggle("active", active === m[0]);
  }
}

function setColorMap(v) {
  setControl("colorMap", v);
  refreshColorMapButtons();
  sendLedDebounced(["colorMap"]);
}

function setCalibrationTest(hue, saturation=255) {
  setControl("globalBrightness", 255);
  setControl("globalHue", 0);
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_mode`, 9);
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_brightness`, 255);
    setControl(`s${s}_hue`, hue);
    setControl(`s${s}_saturation`, saturation);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
}

function resetCalibration() {
  setControl("colorMap", 0);
  setControl("redScale", 255);
  setControl("greenScale", 255);
  setControl("blueScale", 255);
  setControl("gamma10", 10);
  refreshColorMapButtons();
  sendLedNow(true);
  showToast("Calibration reset to RGB / 255 / gamma 1.0");
}

function makeModeButtons(s) {
  const group = (name, title, note) => {
    const buttons = ledModes.filter(m => m[2] === name).map(m =>
      `<button type="button" class="choice-btn" id="s${s}_modeBtn_${m[0]}" onclick="selectMode(${s},${m[0]})">${m[1]}</button>`
    ).join("");
    return `<div class="mode-group-title">${title}</div><div class="mode-note">${note}</div><div class="mode-group-grid">${buttons}</div>`;
  };
  return group("general", "General / No Audio Required", "Reliable ambient modes that keep intentional dark space.") +
         group("rave", "Rave / EDM / Drum & Bass", "Electronic and audio-energy effects with beams, blocks, bursts, sparks, and black gaps.");
}

function makeColorButtons(s) {
  return colorPresets.map(c => `<button type="button" class="color-btn" id="s${s}_colorBtn_${c[0]}" style="background:${c[3]}; color:${c[0] === 'White' ? '#111' : '#fff'}; text-shadow:${c[0] === 'White' ? 'none' : '0 1px 4px rgba(0,0,0,0.9)'};" onclick="setStrandColor(${s},${c[1]},${c[2]})">${c[0]}</button>`).join("");
}

function makeModeStrandControls() {
  for (let s = 0; s < MAX_STRANDS; s++) {
    const root = document.getElementById(`strandControl_${s}`);
    if (!root) continue;
    root.innerHTML = `
      <div class="strand" id="modeCard_${s}">
        <div class="strand-heading">
          <h2>Strand ${s + 1} Controls</h2>
          <span id="strandState_${s}" class="pill">Active</span>
        </div>
        <p class="small">Only this strand is changed from this page. Hardware pin, LED count, type, and color calibration stay on the Hardware page.</p>
        <div class="toggle-row">
          <label>Enabled</label>
          <input id="s${s}_enabled" type="range" min="0" max="1" step="1">
        </div>
        <span id="s${s}_enabledVal" class="pill">0</span>
        <input id="s${s}_mode" class="hidden-input" type="hidden" value="0">
        <div class="section-title">Mode: <span id="s${s}_modeVal" class="pill">Off</span></div>
        <div>${makeModeButtons(s)}</div>
        <div class="section-title">Color</div>
        <div class="color-grid">${makeColorButtons(s)}</div>
        <div class="compact-controls grid">
          <label>Brightness</label><input id="s${s}_brightness" type="range" min="0" max="255" step="1"><span id="s${s}_brightnessVal"></span>
          <label>Hue</label><input id="s${s}_hue" type="range" min="0" max="255" step="1"><span id="s${s}_hueVal"></span>
          <label>Saturation</label><input id="s${s}_saturation" type="range" min="0" max="255" step="1"><span id="s${s}_saturationVal"></span>
          <label>Speed</label><input id="s${s}_speed" type="range" min="0" max="255" step="1"><span id="s${s}_speedVal"></span>
          <label>Density</label><input id="s${s}_density" type="range" min="0" max="255" step="1"><span id="s${s}_densityVal"></span>
          <label>Audio sensitivity</label><input id="s${s}_sensitivity" type="range" min="0" max="255" step="1"><span id="s${s}_sensitivityVal"></span>
          <label>Strobe/decay</label><input id="s${s}_strobe" type="range" min="0" max="255" step="1"><span id="s${s}_strobeVal"></span>
          <label>Twinkle/glitter</label><input id="s${s}_twinkle" type="range" min="0" max="255" step="1"><span id="s${s}_twinkleVal"></span>
          <label>Direction</label><input id="s${s}_direction" type="range" min="0" max="2" step="1"><span id="s${s}_directionVal"></span>
        </div>
        <div class="front-actions">
          <button onclick="sendLedNow(true)">Apply Strand ${s + 1}</button>
          <button onclick="saveAll()">Save Configuration</button>
        </div>
      </div>`;
  }
}

function makeHardwareStrandControls() {
  const root = document.getElementById("hardwareStrandControls");
  root.innerHTML = "";
  for (let s = 0; s < MAX_STRANDS; s++) {
    let typeOptions = ledTypes.map(t => `<option value="${t[0]}">${t[1]}</option>`).join("");
    root.innerHTML += `
      <div class="strand" id="hwCard_${s}">
        <h3>Strand ${s + 1} Hardware</h3>
        <div class="grid">
          <label>LED data GPIO</label><input id="s${s}_pin" type="number" min="0" max="48" step="1"><span id="s${s}_pinVal"></span>
          <label>LED count</label><input id="s${s}_ledCount" type="number" min="0" max="1000" step="1"><span id="s${s}_ledCountVal"></span>
          <label>LED type</label><select id="s${s}_ledType">${typeOptions}</select><span id="s${s}_ledTypeVal"></span>
        </div>
        <p class="small">Your proven WS2815/WS281x setup uses RGB at 800kHz. Other presets remain available for different strands.</p>
      </div>`;
  }
}

function refreshModeButtons(s) {
  const mode = readControl(`s${s}_mode`);
  const label = document.getElementById(`s${s}_modeVal`);
  if (label) label.textContent = modeName(mode);
  for (const m of ledModes) {
    const btn = document.getElementById(`s${s}_modeBtn_${m[0]}`);
    if (btn) btn.classList.toggle("active", String(m[0]) === String(mode));
  }
}

function refreshColorButtons(s) {
  const hue = parseInt(readControl(`s${s}_hue`)) || 0;
  const sat = parseInt(readControl(`s${s}_saturation`)) || 0;
  for (const c of colorPresets) {
    const btn = document.getElementById(`s${s}_colorBtn_${c[0]}`);
    if (!btn) continue;
    const closeHue = Math.abs(((hue - c[1] + 128 + 256) % 256) - 128) < 8;
    const active = (c[2] === 0 && sat < 20) || (c[2] > 0 && sat > 100 && closeHue);
    btn.classList.toggle("active", active);
  }
}


function refreshAllModeButtons() {
  const active = activeStrandCount();
  const mode0 = readControl("s0_mode");
  let allSame = true;
  for (let s = 1; s < active; s++) {
    if (readControl(`s${s}_mode`) !== mode0) allSame = false;
  }
  for (const m of ledModes) {
    const btn = document.getElementById(`allModeBtn_${m[0]}`);
    if (btn) btn.classList.toggle("active", allSame && String(m[0]) === String(mode0));
  }
}

function refreshAllColorButtons() {
  const hue = parseInt(readControl("s0_hue")) || 0;
  const sat = parseInt(readControl("s0_saturation")) || 0;
  const active = activeStrandCount();
  let allSame = true;
  for (let s = 1; s < active; s++) {
    if (readControl(`s${s}_hue`) !== readControl("s0_hue") || readControl(`s${s}_saturation`) !== readControl("s0_saturation")) allSame = false;
  }
  for (const c of colorPresets) {
    const btn = document.getElementById(`allColorBtn_${c[0]}`);
    if (!btn) continue;
    const closeHue = Math.abs(((hue - c[1] + 128 + 256) % 256) - 128) < 8;
    const activeColor = allSame && ((c[2] === 0 && sat < 20) || (c[2] > 0 && sat > 100 && closeHue));
    btn.classList.toggle("active", activeColor);
  }
}

function syncAllControlsFromStrand0() {
  for (const f of allControlFields) {
    setControl(`all_${f}`, readControl(`s0_${f}`));
  }
}

function refreshAllButtons() {
  for (let s = 0; s < MAX_STRANDS; s++) {
    refreshModeButtons(s);
    refreshColorButtons(s);
  }
  refreshAllModeButtons();
  refreshAllColorButtons();
  refreshStrandVisibility();
}

function refreshStrandVisibility() {
  const active = Math.max(1, Math.min(MAX_STRANDS, parseInt(readControl("activeStrands")) || 1));
  for (let s = 0; s < MAX_STRANDS; s++) {
    const isActive = s < active;
    const button = document.getElementById(`btn_strand${s}`);
    const state = document.getElementById(`strandState_${s}`);
    if (button) button.classList.toggle("inactive-strand", !isActive);
    if (state) {
      state.textContent = isActive ? "Active strand" : "Not in Active Strands";
      state.classList.toggle("warn", !isActive);
    }
  }
}

function selectMode(s, mode) {
  setControl(`s${s}_mode`, mode);
  const changed = applyModeDefaultsToStrand(s, mode);
  refreshModeButtons(s);
  if (s === 0) syncAllControlsFromStrand0();
  sendLedDebounced(changed);
}

function setStrandColor(s, hue, saturation) {
  setControl(`s${s}_hue`, hue);
  setControl(`s${s}_saturation`, saturation);
  refreshColorButtons(s);
  sendLedDebounced([`s${s}_hue`, `s${s}_saturation`]);
}


function selectAllMode(mode) {
  const active = activeStrandCount();
  const ids = [];
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, mode);
    ids.push(`s${s}_enabled`);
    ids.push(...applyModeDefaultsToStrand(s, mode));
    refreshModeButtons(s);
  }
  refreshAllModeButtons();
  syncAllControlsFromStrand0();
  sendLedDebounced(ids);
}

function setAllColor(hue, saturation) {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_hue`, hue);
    setControl(`s${s}_saturation`, saturation);
    refreshColorButtons(s);
  }
  refreshAllColorButtons();
  const ids = [];
  for (let s = 0; s < active; s++) ids.push(`s${s}_enabled`, `s${s}_hue`, `s${s}_saturation`);
  sendLedDebounced(ids);
}

function applyAllField(field, value) {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_${field}`, value);
    const label = document.getElementById(`s${s}_${field}Val`);
    if (label) label.textContent = value;
  }
  refreshAllButtons();
  const ids = [];
  for (let s = 0; s < active; s++) ids.push(`s${s}_${field}`);
  sendLedDebounced(ids);
}

function copyStrandOneToAll() {
  const active = activeStrandCount();
  for (let s = 1; s < active; s++) {
    for (const f of modeFields) {
      setControl(`s${s}_${f}`, readControl(`s0_${f}`));
    }
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
}

function setGlobalColor(hue) {
  setControl("globalHue", hue);
  sendLedDebounced(["globalHue"]);
}

async function applyCleanAudioBaseline() {
  const values = {
    shiftTo16:16,
    volumeGain:0.90, bassGain:1.15, midGain:1.05, trebleGain:1.45, lrGain:0.55,
    volumeFloor:44, bassFloor:70, midFloor:126, trebleFloor:83,
    attack:0.60, release:0.08, peakDecay:0.995,
    beatVolMin:35, beatBassMin:45,
    beatSensitivity:0.85, beatMinOnset:0.018,
    beatTempoAssist:1, beatPredictStrength:0.70, beatPredictToleranceMs:125,
    beatCooldownMs:210, beatHoldMs:85, beatMinBpm:70, beatMaxBpm:190,
    packetIntervalMs:33
  };
  for (const [id, value] of Object.entries(values)) setControl(id, value);
  updateSettingsText();
  const params = new URLSearchParams();
  addFullTuningConfig(params);
  try {
    const response = await fetch("/api/setTuning?" + params.toString(), {cache:"no-store"});
    if (!response.ok) throw new Error("baseline was not accepted");
    await fetch("/api/resetAgc", {cache:"no-store"});
    await fetch("/api/resetBeat", {cache:"no-store"});
    showToast("Clean audio baseline loaded; tune floors from silence first");
  } catch (e) {
    showToast("Baseline failed: " + e.message, true);
  }
}

function setFourMeterLayout() {
  const modes = [6, 11, 12, 13];
  const hues = [96, 8, 96, 175];
  const active = activeStrandCount();
  for (let s = 0; s < MAX_STRANDS; s++) {
    setControl(`s${s}_mode`, modes[s]);
    setControl(`s${s}_brightness`, 230);
    setControl(`s${s}_hue`, hues[s]);
    setControl(`s${s}_saturation`, 255);
    setControl(`s${s}_sensitivity`, 128);
    setControl(`s${s}_strobe`, 150);
    if (s < active) setControl(`s${s}_enabled`, 1);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("VU layout applied: Overall / Bass / Mid / Treble");
}

function setVuPreview(mode, label) {
  setControl("s0_enabled", 1);
  setControl("s0_mode", mode);
  setControl("s0_brightness", 230);
  setControl("s0_sensitivity", 128);
  setControl("s0_strobe", 150);
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast(`Strand 1 set to ${label}`);
}

function setAllOverallVu() {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, 6);
    setControl(`s${s}_brightness`, 230);
    setControl(`s${s}_sensitivity`, 128);
    setControl(`s${s}_strobe`, 150);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("Overall VU applied to all active strands");
}

function restoreSolidColor() {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, 9);
    applyModeDefaultsToStrand(s, 9);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("Solid Color restored on all active strands");
}

function installControls() {
  for (const id of tuningIds) {
    const el = document.getElementById(id);
    if (!el) continue;
    el.addEventListener("input", () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      sendTuningDebounced(id);
      updateSettingsText();
    });
  }
  for (const id of ["activeStrands","maxLedsPerStrand","globalBrightness","globalHue","redScale","greenScale","blueScale","gamma10"]) {
    const el = document.getElementById(id);
    if (!el) continue;
    const update = () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      refreshAllButtons();
      sendLedDebounced([id]);
    };
    el.addEventListener("input", update);
    el.addEventListener("change", update);
  }
  for (const f of allControlFields) {
    const id = `all_${f}`;
    const el = document.getElementById(id);
    if (!el) continue;
    const update = () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      applyAllField(f, el.value);
    };
    el.addEventListener("input", update);
    el.addEventListener("change", update);
  }

  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) {
      const id = `s${s}_${f}`;
      const el = document.getElementById(id);
      if (!el) continue;
      const update = () => {
        const label = document.getElementById(id + "Val");
        if (label) label.textContent = el.value;
        if (f === "mode") refreshModeButtons(s);
        if (f === "hue" || f === "saturation") refreshColorButtons(s);
        if (s === 0 && allControlFields.includes(f)) syncAllControlsFromStrand0();
        refreshAllModeButtons();
        refreshAllColorButtons();
        sendLedDebounced([id]);
      };
      el.addEventListener("input", update);
      el.addEventListener("change", update);
    }
  }
}

function loadControlsFromState(j) {
  const c = j.cfg;
  for (const id of tuningIds) if (c[id] !== undefined) setControl(id, c[id]);
  const l = j.led;
  setControl("activeStrands", l.activeStrands);
  setControl("maxLedsPerStrand", l.maxLedsPerStrand);
  setControl("globalBrightness", l.globalBrightness);
  setControl("globalHue", l.globalHue);
  if (l.redScale !== undefined) setControl("redScale", l.redScale);
  if (l.greenScale !== undefined) setControl("greenScale", l.greenScale);
  if (l.blueScale !== undefined) setControl("blueScale", l.blueScale);
  if (l.gamma10 !== undefined) setControl("gamma10", l.gamma10);
  if (l.colorMap !== undefined) setControl("colorMap", l.colorMap);
  refreshColorMapButtons();
  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) {
      if (l.strand[s][f] !== undefined) setControl(`s${s}_${f}`, l.strand[s][f]);
    }
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  updateSettingsText();
}

function sendTuningDebounced(id=null) {
  if (id) pendingTuningIds.add(id);
  clearTimeout(debounceTune);
  debounceTune = setTimeout(flushTuningQueue, 100);
}

function flushTuningQueue() {
  if (tuningRequestInFlight || pendingTuningIds.size === 0) return;
  const params = new URLSearchParams();
  for (const id of pendingTuningIds) params.set(id, readControl(id));
  pendingTuningIds.clear();
  tuningRequestInFlight = true;
  fetch("/api/setTuning?" + params.toString(), {cache:"no-store"})
    .catch(console.log)
    .finally(() => {
      tuningRequestInFlight = false;
      if (pendingTuningIds.size) debounceTune = setTimeout(flushTuningQueue, 30);
    });
}

function addFullTuningConfig(params) {
  for (const id of tuningIds) params.set(id, readControl(id));
}

function addFullLedConfig(params) {
  params.set("activeStrands", readControl("activeStrands"));
  params.set("maxLedsPerStrand", readControl("maxLedsPerStrand"));
  params.set("globalBrightness", readControl("globalBrightness"));
  params.set("globalHue", readControl("globalHue"));
  params.set("colorMap", readControl("colorMap"));
  params.set("redScale", readControl("redScale"));
  params.set("greenScale", readControl("greenScale"));
  params.set("blueScale", readControl("blueScale"));
  params.set("gamma10", readControl("gamma10"));
  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) params.set(`s${s}_${f}`, readControl(`s${s}_${f}`));
  }
}

function sendLedDebounced(ids=[]) {
  if (!Array.isArray(ids)) ids = [ids];
  for (const id of ids) if (id) pendingLedIds.add(id);
  clearTimeout(debounceLed);
  debounceLed = setTimeout(flushLedQueue, 100);
}

function sendLedNow(full=true) {
  if (full) forceFullLedRequest = true;
  clearTimeout(debounceLed);
  flushLedQueue();
}

function flushLedQueue() {
  if (ledRequestInFlight) return;
  if (!forceFullLedRequest && pendingLedIds.size === 0) return;

  const params = new URLSearchParams();
  if (forceFullLedRequest) {
    addFullLedConfig(params);
  } else {
    for (const id of pendingLedIds) params.set(id, readControl(id));
  }
  pendingLedIds.clear();
  forceFullLedRequest = false;

  ledRequestInFlight = true;
  fetch("/api/setLed?" + params.toString(), {cache:"no-store"})
    .catch(console.log)
    .finally(() => {
      ledRequestInFlight = false;
      if (forceFullLedRequest || pendingLedIds.size) debounceLed = setTimeout(flushLedQueue, 20);
    });
}

function updateSettingsText() {
  const text =
`// Audio tuning values:
cfg.shiftTo16             = ${readControl("shiftTo16")};

cfg.volumeGain            = ${readControl("volumeGain")}f;
cfg.bassGain              = ${readControl("bassGain")}f;
cfg.midGain               = ${readControl("midGain")}f;
cfg.trebleGain            = ${readControl("trebleGain")}f;
cfg.lrGain                = ${readControl("lrGain")}f;

cfg.volumeFloor           = ${readControl("volumeFloor")}f;
cfg.bassFloor             = ${readControl("bassFloor")}f;
cfg.midFloor              = ${readControl("midFloor")}f;
cfg.trebleFloor           = ${readControl("trebleFloor")}f;

cfg.attack                = ${readControl("attack")}f;
cfg.release               = ${readControl("release")}f;
cfg.peakDecay             = ${readControl("peakDecay")}f;

cfg.beatVolMin            = ${readControl("beatVolMin")};
cfg.beatBassMin           = ${readControl("beatBassMin")};
cfg.beatSensitivity       = ${readControl("beatSensitivity")}f;
cfg.beatMinOnset          = ${readControl("beatMinOnset")}f;
cfg.beatTempoAssist       = ${readControl("beatTempoAssist")};
cfg.beatPredictStrength   = ${readControl("beatPredictStrength")}f;
cfg.beatPredictToleranceMs= ${readControl("beatPredictToleranceMs")};
cfg.beatCooldownMs        = ${readControl("beatCooldownMs")};
cfg.beatHoldMs            = ${readControl("beatHoldMs")};
cfg.beatMinBpm            = ${readControl("beatMinBpm")};
cfg.beatMaxBpm            = ${readControl("beatMaxBpm")};
cfg.packetIntervalMs      = ${readControl("packetIntervalMs")};`;
  document.getElementById("settingsText").textContent = text;
  return text;
}

function copyAudioSettings() {
  navigator.clipboard.writeText(updateSettingsText()).then(() => alert("Audio settings copied."));
}

let toastTimer = null;
function showToast(message, isError=false) {
  const el = document.getElementById("toast");
  if (!el) return;
  el.textContent = message;
  el.classList.toggle("error", isError);
  el.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove("show"), 2600);
}

function sleepMs(ms) { return new Promise(resolve => setTimeout(resolve, ms)); }

async function saveAll() {
  try {
    // Cancel pending debounce timers so the save cannot race an older slider request.
    clearTimeout(debounceLed);
    clearTimeout(debounceTune);
    pendingLedIds.clear();
    pendingTuningIds.clear();
    forceFullLedRequest = false;

    const waitStarted = Date.now();
    while ((ledRequestInFlight || tuningRequestInFlight) && Date.now() - waitStarted < 2500) {
      await sleepMs(25);
    }

    showToast("Applying current settings...");

    const ledParams = new URLSearchParams();
    addFullLedConfig(ledParams);
    const ledResponse = await fetch("/api/setLed?" + ledParams.toString(), {cache:"no-store"});
    if (!ledResponse.ok) throw new Error("LED settings were not accepted");

    const tuningParams = new URLSearchParams();
    addFullTuningConfig(tuningParams);
    const tuningResponse = await fetch("/api/setTuning?" + tuningParams.toString(), {cache:"no-store"});
    if (!tuningResponse.ok) throw new Error("Audio settings were not accepted");

    const saveResponse = await fetch("/api/save", {cache:"no-store"});
    const result = await saveResponse.json();
    if (!saveResponse.ok || !result.saved) throw new Error(result.error || "Flash verification failed");

    showToast(`Saved: ${result.activeStrands} active strand${result.activeStrands === 1 ? "" : "s"}`);
  } catch (e) {
    console.log(e);
    showToast("Save failed: " + e.message, true);
  }
}
function resetAgc() { fetch("/api/resetAgc"); }
function resetBeat() { fetch("/api/resetBeat"); }
function loadDefaults() { if (confirm("Load defaults for audio and LED settings?")) fetch("/api/defaults").then(() => { loadedOnce = false; setTimeout(updateState, 250); }); }

function setBeatLed(id, on) {
  const el = document.getElementById(id);
  if (!el) return;
  if (on) el.classList.add("on"); else el.classList.remove("on");
}

function applyLiveState(j) {
  for (const name of meterNames) setMeter("meters_tuning", name, j[name]);
  if (lastUiBeatPulse === null) {
    lastUiBeatPulse = j.beatPulse;
  } else if (j.beatPulse !== lastUiBeatPulse) {
    lastUiBeatPulse = j.beatPulse;
    setBeatLed("beatLightFinal", true);
    clearTimeout(uiBeatTimer);
    uiBeatTimer = setTimeout(() => setBeatLed("beatLightFinal", false), 140);
  } else if (!j.beat) {
    setBeatLed("beatLightFinal", false);
  }
  setBeatLed("beatLightCandidate", j.beatCandidate > 0);

  const rawEl = document.getElementById("rawText");
  if (rawEl) rawEl.textContent = `raw volume=${j.rawVolume.toFixed(1)} bass=${j.rawBass.toFixed(1)} mid=${j.rawMid.toFixed(1)} treble=${j.rawTreble.toFixed(1)} clip=${j.clip}`;

  const beatDebugEl = document.getElementById("beatDebugText");
  if (beatDebugEl) beatDebugEl.textContent = `FFT=${j.fftReady ? "ready" : "filling"} ${j.fftSize}pt peak=${j.fftPeakHz.toFixed(0)}Hz onset=${j.beatOnset.toFixed(3)} threshold=${j.beatThreshold.toFixed(3)} score=${j.beatScore.toFixed(2)} flux=${j.spectralFlux.toFixed(3)} low=${j.lowFlux.toFixed(3)} high=${j.highFlux.toFixed(3)} BPM=${j.beatBpm.toFixed(1)} confidence=${j.beatConfidence || 0} phase=${j.beatPhase || 0} age=${j.beatAge.toFixed(0)}ms candidate=${j.beatCandidate} tempoAssist=${j.beatTempoAssist}`;

  const map = {
    pps:j.pps, enqOk:j.enqOk, enqErr:j.enqErr, busySkip:j.busySkip,
    cbOk:j.cbOk, cbFail:j.cbFail, lastErr:j.lastErr,
    wifiRssi:`${j.wifiRssi} dBm`, wifiIp:j.wifiIp, wifiChannel:j.wifiChannel,
    freeHeap:j.freeHeap, fftUs:`${j.fftUs}/${j.fftAvgUs}`,
    analyzeUs:j.analyzeUs, httpUs:j.httpUs, loopUs:j.loopUs
  };
  for (const [id,value] of Object.entries(map)) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
  }

}

async function loadInitialState() {
  try {
    const r = await fetch("/api/state", {cache:"no-store"});
    const j = await r.json();
    loadControlsFromState(j);
    loadedOnce = true;
    applyLiveState(j);
  } catch (e) {
    console.log(e);
  }
}

function livePollDelay() {
  if (document.hidden) return 2500;
  if (activeTab === "tuning") return 200;
  if (activeTab === "hardware") return 600;
  return 1000;
}

function scheduleLivePoll(delay=livePollDelay()) {
  clearTimeout(livePollTimer);
  livePollTimer = setTimeout(pollLiveState, delay);
}

async function pollLiveState() {
  if (livePollInFlight) {
    scheduleLivePoll();
    return;
  }
  livePollInFlight = true;
  try {
    const r = await fetch("/api/live", {cache:"no-store"});
    const j = await r.json();
    applyLiveState(j);
  } catch (e) {
    console.log(e);
  } finally {
    livePollInFlight = false;
    scheduleLivePoll();
  }
}

// Backward-compatible helper used after defaults actions.
function updateState() {
  if (!loadedOnce) return loadInitialState();
  return pollLiveState();
}

document.addEventListener("visibilitychange", () => scheduleLivePoll(0));

makeMeters("meters_tuning");
makeGlobalColorButtons();
makeAllModeButtons();
makeAllColorButtons();
makeColorMapButtons();
makeModeStrandControls();
makeHardwareStrandControls();
installControls();
showTab("all");
loadInitialState().finally(() => scheduleLivePoll(0));
</script>
</body>
</html>
)rawliteral";


// ============================================================
// Helpers
// ============================================================
static float clamp_float(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static uint8_t clamp_u8(float v) {
  if (v < 0.0f) return 0;
  if (v > 255.0f) return 255;
  return (uint8_t)(v + 0.5f);
}

static int16_t sample32_to_s16(int32_t s) {
  int shift = clamp_int(cfg.shiftTo16, 12, 20);
  int32_t v = s >> shift;

  if (v > 32767) return 32767;
  if (v < -32768) return -32768;

  return (int16_t)v;
}



static void set_default_led_config() {
  // Proven receiver wiring from the completed diagnostics:
  //   Strand 1 -> GPIO33
  //   Strand 2 -> GPIO25
  //   Strand 3 -> GPIO26
  //   Strand 4 -> GPIO27
  // Proven native LED byte order: RGB at 800 kHz.
  // Browser hardware settings are still available for counts, enable flags, and future strip types.
  ledCfg.globalBrightness = 255;
  ledCfg.globalHue = 0;
  ledCfg.activeStrands = MAX_STRANDS;
  ledCfg.maxLedsPerStrand = 300;

  // Start with a neutral RGB calibration map because the receiver's native
  // NeoPixel type now matches the proven RGB byte order. You can still fine-tune
  // channel scales and gamma from the Hardware / Color Calibration section.
  ledCfg.colorMap = COLOR_MAP_RGB;
  ledCfg.redScale = 255;
  ledCfg.greenScale = 255;
  ledCfg.blueScale = 255;
  ledCfg.gamma10 = 10;

  // Fresh installs start in a dependable non-audio mode. Additional strands
  // remain disabled until their LED counts and enable states are configured.
  ledCfg.strand[0] = {1, 9, 220, 165, 255, 0, 0, 0, 0, 0, 0, DEFAULT_LED_PINS[0], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[0], 0};
  ledCfg.strand[1] = {0, 15, 210, 190, 255, 48, 118, 0, 175, 24, 0, DEFAULT_LED_PINS[1], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[1], 0};
  ledCfg.strand[2] = {0, 16, 220, 165, 255, 62, 48, 0, 205, 170, 0, DEFAULT_LED_PINS[2], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[2], 0};
  ledCfg.strand[3] = {0, 17, 230, 200, 255, 118, 76, 0, 185, 8, 0, DEFAULT_LED_PINS[3], LED_TYPE_WS2815_WS2812_RGB_800, 0, DEFAULT_LED_COUNTS[3], 0};
}

static void set_default_config() {
  // Conservative clean baseline derived from the proven PCM1808/FLX4 setup.
  // Start here, tune floors from silence first, then tune gains with the new
  // Overall/Bass/Mid/Treble VU modes. SHIFT=16 preserves headroom.
  cfg.volumeGain = 0.90f;
  cfg.bassGain = 1.15f;
  cfg.midGain = 1.05f;
  cfg.trebleGain = 1.45f;
  cfg.lrGain = 0.55f;

  cfg.volumeFloor = 44.0f;
  cfg.bassFloor = 70.0f;
  cfg.midFloor = 126.0f;
  cfg.trebleFloor = 83.0f;

  cfg.attack = 0.60f;
  cfg.release = 0.08f;
  cfg.peakDecay = 0.995f;

  cfg.beatVolMin = 35;
  cfg.beatBassMin = 45;
  cfg.beatSensitivity = 0.85f;
  cfg.beatMinOnset = 0.018f;
  cfg.beatTempoAssist = 1;
  cfg.beatPredictStrength = 0.70f;
  cfg.beatPredictToleranceMs = 125;
  cfg.beatCooldownMs = 210;
  cfg.beatHoldMs = 85;
  cfg.beatMinBpm = 70;
  cfg.beatMaxBpm = 190;

  cfg.packetIntervalMs = 33;
  cfg.shiftTo16 = 16;

  set_default_led_config();

}

static void reset_agc() {
  peak_volume = 1200.0f;
  peak_bass = 300.0f;
  peak_mid = 300.0f;
  peak_treble = 300.0f;
  peak_left = 1200.0f;
  peak_right = 1200.0f;

  smooth_volume = 0.0f;
  smooth_bass = 0.0f;
  smooth_mid = 0.0f;
  smooth_treble = 0.0f;
  smooth_left = 0.0f;
  smooth_right = 0.0f;

  lp40 = 0.0f;
  lp160 = 0.0f;
  lp250 = 0.0f;
  lp4000 = 0.0f;
}

static void reset_beat_state() {
  prevLogVolume = 0.0f;
  beatStatePrimed = false;
  beatWarmupBlocks = 0;
  onsetMean = 0.0f;
  onsetDev = 0.020f;
  onsetPrev2 = 0.0f;
  onsetPrev1 = 0.0f;
  thresholdPrev2 = 0.0f;
  thresholdPrev1 = 0.0f;
  onsetMsPrev2 = 0;
  onsetMsPrev1 = 0;
  lastBeatMs = 0;
  beatHoldUntilMs = 0;
  beatPeriodMs = 0.0f;
  debugOnset = 0.0f;
  debugThreshold = 0.0f;
  debugScore = 0.0f;
  debugBpm = 0.0f;
  debugBeatAgeMs = 0.0f;
  debugBeatPeriodMs = 0.0f;
  debugCandidate = 0;
  debugTempoAssist = 0;
  debugSpectralFlux = 0.0f;
  debugLowFlux = 0.0f;
  debugHighFlux = 0.0f;
  debugFftPeakHz = 0.0f;
  debugFftReady = 0;
  fftFilledSamples = 0;
  fftReady = false;
  fftBandPrimed = false;
  fftBlockCounter = 0;
  memset(fftMonoWindow, 0, sizeof(fftMonoWindow));
  memset(fftReal, 0, sizeof(fftReal));
  memset(fftImag, 0, sizeof(fftImag));
  for (int i = 0; i < FFT_BAND_COUNT; i++) {
    fftBandLog[i] = 0.0f;
    prevBandLog[i] = 0.0f;
  }
}

static uint32_t config_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static uint32_t persisted_blob_crc(const PersistedConfigBlob &blob) {
  return config_crc32(
    reinterpret_cast<const uint8_t *>(&blob),
    offsetof(PersistedConfigBlob, crc32)
  );
}

static bool persisted_blob_valid(const PersistedConfigBlob &blob) {
  return blob.magic == PERSIST_MAGIC &&
         blob.version == PERSIST_VERSION &&
         blob.size == sizeof(PersistedConfigBlob) &&
         blob.crc32 == persisted_blob_crc(blob);
}

static void sanitize_loaded_config() {
  ledCfg.activeStrands = (uint8_t)clamp_int(ledCfg.activeStrands, 1, MAX_STRANDS);
  ledCfg.maxLedsPerStrand = (uint16_t)clamp_int(ledCfg.maxLedsPerStrand, 1, 1000);
  ledCfg.colorMap = (uint8_t)clamp_int(ledCfg.colorMap, 0, 5);
  ledCfg.gamma10 = (uint8_t)clamp_int(ledCfg.gamma10, 10, 30);

  cfg.shiftTo16 = clamp_int(cfg.shiftTo16, 12, 20);
  cfg.packetIntervalMs = (uint16_t)clamp_int(cfg.packetIntervalMs, 15, 500);
  cfg.beatPredictToleranceMs = (uint16_t)clamp_int(cfg.beatPredictToleranceMs, 20, 500);
  cfg.beatCooldownMs = (uint16_t)clamp_int(cfg.beatCooldownMs, 40, 1000);
  cfg.beatHoldMs = (uint16_t)clamp_int(cfg.beatHoldMs, 10, 1000);
  cfg.beatMinBpm = (uint16_t)clamp_int(cfg.beatMinBpm, 30, 240);
  cfg.beatMaxBpm = (uint16_t)clamp_int(cfg.beatMaxBpm, cfg.beatMinBpm + 1, 300);

  for (int s = 0; s < MAX_STRANDS; ++s) {
    ledCfg.strand[s].enabled = ledCfg.strand[s].enabled ? 1 : 0;
    ledCfg.strand[s].mode = (uint8_t)clamp_int(ledCfg.strand[s].mode, 0, 31);
    ledCfg.strand[s].pin = (uint8_t)clamp_int(ledCfg.strand[s].pin, 0, 48);
    ledCfg.strand[s].ledType = (uint8_t)clamp_int(ledCfg.strand[s].ledType, 0, 6);
    ledCfg.strand[s].direction = (uint8_t)clamp_int(ledCfg.strand[s].direction, 0, 2);
    ledCfg.strand[s].ledCount = (uint16_t)clamp_int(
      ledCfg.strand[s].ledCount,
      0,
      ledCfg.maxLedsPerStrand
    );
    ledCfg.strand[s].reserved8 = 0;
    ledCfg.strand[s].reserved = 0;
  }
}

static bool read_persisted_slot(const char *key, PersistedConfigBlob &blob) {
  memset(&blob, 0, sizeof(blob));

  Preferences store;
  if (!store.begin(PERSIST_NAMESPACE, false)) {
    return false;
  }

  const size_t storedLength = store.getBytesLength(key);
  size_t readLength = 0;
  if (storedLength == sizeof(PersistedConfigBlob)) {
    readLength = store.getBytes(key, &blob, sizeof(blob));
  }
  store.end();

  return readLength == sizeof(PersistedConfigBlob) && persisted_blob_valid(blob);
}

static bool load_persisted_config() {
  PersistedConfigBlob slotA = {};
  PersistedConfigBlob slotB = {};
  const bool validA = read_persisted_slot(PERSIST_SLOT_A, slotA);
  const bool validB = read_persisted_slot(PERSIST_SLOT_B, slotB);

  if (!validA && !validB) {
    return false;
  }

  const PersistedConfigBlob *selected = nullptr;
  const char *selectedName = nullptr;

  if (validA && (!validB || (int32_t)(slotA.sequence - slotB.sequence) > 0)) {
    selected = &slotA;
    selectedName = PERSIST_SLOT_A;
  } else {
    selected = &slotB;
    selectedName = PERSIST_SLOT_B;
  }

  cfg = selected->tuning;
  ledCfg = selected->leds;
  persistedConfigSequence = selected->sequence;
  sanitize_loaded_config();

  Serial.printf(
    "Loaded saved configuration slot %s sequence=%lu: activeStrands=%u RGBscale=[%u,%u,%u] colorMap=%u\n",
    selectedName,
    (unsigned long)persistedConfigSequence,
    ledCfg.activeStrands,
    ledCfg.redScale,
    ledCfg.greenScale,
    ledCfg.blueScale,
    ledCfg.colorMap
  );
  return true;
}

static bool clear_legacy_storage() {
  bool allCleared = true;
  uint8_t clearedCount = 0;

  for (size_t i = 0; i < sizeof(LEGACY_NAMESPACES) / sizeof(LEGACY_NAMESPACES[0]); ++i) {
    const char *name = LEGACY_NAMESPACES[i];

    // Probe read-only first so a missing historical namespace is not recreated.
    Preferences probe;
    if (!probe.begin(name, true)) {
      continue;
    }
    probe.end();

    Preferences legacy;
    if (!legacy.begin(name, false)) {
      allCleared = false;
      continue;
    }

    const bool cleared = legacy.clear();
    legacy.end();
    if (cleared) {
      ++clearedCount;
    } else {
      allCleared = false;
      Serial.printf("WARNING: could not clear legacy namespace %s.\n", name);
    }
  }

  Serial.printf("Cleared %u legacy Lumina configuration namespace(s).\n", clearedCount);
  return allCleared;
}

static void load_legacy_config() {
  if (!prefs.begin(LEGACY_NAMESPACE, true)) {
    Serial.println("No legacy saved configuration found; using defaults.");
    configMigrationNeeded = true;
    return;
  }

  const uint8_t storedConfigVersion = prefs.getUChar("cfgVer", 0);

  cfg.volumeGain = prefs.getFloat("volGain", cfg.volumeGain);
  cfg.bassGain = prefs.getFloat("bassGain", cfg.bassGain);
  cfg.midGain = prefs.getFloat("midGain", cfg.midGain);
  cfg.trebleGain = prefs.getFloat("trebGain", cfg.trebleGain);
  cfg.lrGain = prefs.getFloat("lrGain", cfg.lrGain);

  cfg.volumeFloor = prefs.getFloat("volFloor", cfg.volumeFloor);
  cfg.bassFloor = prefs.getFloat("bassFloor", cfg.bassFloor);
  cfg.midFloor = prefs.getFloat("midFloor", cfg.midFloor);
  cfg.trebleFloor = prefs.getFloat("trebFloor", cfg.trebleFloor);

  cfg.attack = prefs.getFloat("attack", cfg.attack);
  cfg.release = prefs.getFloat("release", cfg.release);
  cfg.peakDecay = prefs.getFloat("decay", cfg.peakDecay);

  cfg.beatVolMin = prefs.getUChar("beatVol", cfg.beatVolMin);
  cfg.beatBassMin = prefs.getUChar("beatBass", cfg.beatBassMin);
  cfg.beatSensitivity = prefs.getFloat("beatSens", cfg.beatSensitivity);
  cfg.beatMinOnset = prefs.getFloat("beatOnset", cfg.beatMinOnset);
  cfg.beatTempoAssist = prefs.getUChar("beatAssist", cfg.beatTempoAssist);
  cfg.beatPredictStrength = prefs.getFloat("beatPred", cfg.beatPredictStrength);
  cfg.beatPredictToleranceMs = prefs.getUShort("beatTol", cfg.beatPredictToleranceMs);
  cfg.beatCooldownMs = prefs.getUShort("beatCool", cfg.beatCooldownMs);
  cfg.beatHoldMs = prefs.getUShort("beatHold", cfg.beatHoldMs);
  cfg.beatMinBpm = prefs.getUShort("beatMinBpm", cfg.beatMinBpm);
  cfg.beatMaxBpm = prefs.getUShort("beatMaxBpm", cfg.beatMaxBpm);

  cfg.packetIntervalMs = prefs.getUShort("pktMs", cfg.packetIntervalMs);
  cfg.shiftTo16 = prefs.getInt("shift", cfg.shiftTo16);

  ledCfg.globalBrightness = prefs.getUChar("gBright", ledCfg.globalBrightness);
  ledCfg.globalHue = prefs.getUChar("gHue", ledCfg.globalHue);
  ledCfg.activeStrands = prefs.getUChar("activeS", ledCfg.activeStrands);
  ledCfg.maxLedsPerStrand = prefs.getUShort("maxLeds", ledCfg.maxLedsPerStrand);
  ledCfg.colorMap = prefs.getUChar("colorMap", ledCfg.colorMap);
  ledCfg.redScale = prefs.getUChar("rScale", ledCfg.redScale);
  ledCfg.greenScale = prefs.getUChar("gScale", ledCfg.greenScale);
  ledCfg.blueScale = prefs.getUChar("bScale", ledCfg.blueScale);
  ledCfg.gamma10 = prefs.getUChar("gamma10", ledCfg.gamma10);

  for (int s = 0; s < MAX_STRANDS; s++) {
    char key[20];
    snprintf(key, sizeof(key), "s%den", s); ledCfg.strand[s].enabled = prefs.getUChar(key, ledCfg.strand[s].enabled);
    snprintf(key, sizeof(key), "s%dmode", s); ledCfg.strand[s].mode = prefs.getUChar(key, ledCfg.strand[s].mode);
    snprintf(key, sizeof(key), "s%dbr", s); ledCfg.strand[s].brightness = prefs.getUChar(key, ledCfg.strand[s].brightness);
    snprintf(key, sizeof(key), "s%dhue", s); ledCfg.strand[s].hue = prefs.getUChar(key, ledCfg.strand[s].hue);
    snprintf(key, sizeof(key), "s%dsat", s); ledCfg.strand[s].saturation = prefs.getUChar(key, ledCfg.strand[s].saturation);
    snprintf(key, sizeof(key), "s%dspd", s); ledCfg.strand[s].speed = prefs.getUChar(key, ledCfg.strand[s].speed);
    snprintf(key, sizeof(key), "s%dden", s); ledCfg.strand[s].density = prefs.getUChar(key, ledCfg.strand[s].density);
    snprintf(key, sizeof(key), "s%dsens", s); ledCfg.strand[s].sensitivity = prefs.getUChar(key, ledCfg.strand[s].sensitivity);
    snprintf(key, sizeof(key), "s%dstr", s); ledCfg.strand[s].strobe = prefs.getUChar(key, ledCfg.strand[s].strobe);
    snprintf(key, sizeof(key), "s%dtwi", s); ledCfg.strand[s].twinkle = prefs.getUChar(key, ledCfg.strand[s].twinkle);
    snprintf(key, sizeof(key), "s%ddir", s); ledCfg.strand[s].direction = prefs.getUChar(key, ledCfg.strand[s].direction);
    snprintf(key, sizeof(key), "s%dpin", s); ledCfg.strand[s].pin = prefs.getUChar(key, ledCfg.strand[s].pin);
    snprintf(key, sizeof(key), "s%dcnt", s); ledCfg.strand[s].ledCount = prefs.getUShort(key, ledCfg.strand[s].ledCount);
    snprintf(key, sizeof(key), "s%dtyp", s); ledCfg.strand[s].ledType = prefs.getUChar(key, ledCfg.strand[s].ledType);
  }

  prefs.end();

  if (storedConfigVersion < CONFIG_SCHEMA_VERSION) {
    // Preserve tuning/counts but correct the proven receiver wiring and native color order.
    ledCfg.colorMap = COLOR_MAP_RGB;
    for (int s = 0; s < MAX_STRANDS; s++) {
      ledCfg.strand[s].pin = DEFAULT_LED_PINS[s];
      ledCfg.strand[s].ledType = LED_TYPE_WS2815_WS2812_RGB_800;
    }
    if (cfg.packetIntervalMs == 27) cfg.packetIntervalMs = 33;
  }

  sanitize_loaded_config();
  configMigrationNeeded = true;

  Serial.printf(
    "Loaded legacy configuration for one-time migration: activeStrands=%u RGBscale=[%u,%u,%u] colorMap=%u\n",
    ledCfg.activeStrands,
    ledCfg.redScale,
    ledCfg.greenScale,
    ledCfg.blueScale,
    ledCfg.colorMap
  );
}

static void load_config() {
  set_default_config();
  configMigrationNeeded = false;

  if (load_persisted_config()) {
    return;
  }

  load_legacy_config();
}

static bool write_persisted_slot(const char *key, const PersistedConfigBlob &blob) {
  Preferences store;
  if (!store.begin(PERSIST_NAMESPACE, false)) {
    Serial.println("ERROR: could not open compact configuration namespace for writing.");
    return false;
  }

  // We only overwrite the older slot, so removing it cannot destroy the latest
  // valid copy. Removing first also avoids any stale key-type conflict.
  if (store.isKey(key)) {
    store.remove(key);
  }

  const size_t written = store.putBytes(key, &blob, sizeof(blob));
  store.end();

  if (written != sizeof(blob)) {
    Serial.printf(
      "ERROR: Preferences putBytes(%s) wrote %u of %u bytes. NVS may be full or fragmented.\n",
      key,
      (unsigned)written,
      (unsigned)sizeof(blob)
    );
    return false;
  }

  PersistedConfigBlob verify = {};
  if (!read_persisted_slot(key, verify)) {
    Serial.printf("ERROR: saved slot %s failed CRC/read-back validation.\n", key);
    return false;
  }

  if (verify.sequence != blob.sequence ||
      verify.leds.activeStrands != blob.leds.activeStrands ||
      verify.leds.redScale != blob.leds.redScale ||
      verify.leds.greenScale != blob.leds.greenScale ||
      verify.leds.blueScale != blob.leds.blueScale) {
    Serial.printf("ERROR: saved slot %s read-back values did not match.\n", key);
    return false;
  }

  return true;
}

static bool save_config() {
  sanitize_loaded_config();

  PersistedConfigBlob slotA = {};
  PersistedConfigBlob slotB = {};
  const bool validA = read_persisted_slot(PERSIST_SLOT_A, slotA);
  const bool validB = read_persisted_slot(PERSIST_SLOT_B, slotB);

  uint32_t newestSequence = persistedConfigSequence;
  if (validA && (int32_t)(slotA.sequence - newestSequence) > 0) newestSequence = slotA.sequence;
  if (validB && (int32_t)(slotB.sequence - newestSequence) > 0) newestSequence = slotB.sequence;

  const char *targetSlot;
  if (!validA) {
    targetSlot = PERSIST_SLOT_A;
  } else if (!validB) {
    targetSlot = PERSIST_SLOT_B;
  } else {
    // Overwrite the older slot and leave the newest valid slot untouched.
    targetSlot = ((int32_t)(slotA.sequence - slotB.sequence) <= 0)
      ? PERSIST_SLOT_A
      : PERSIST_SLOT_B;
  }

  PersistedConfigBlob blob = {};
  blob.magic = PERSIST_MAGIC;
  blob.version = PERSIST_VERSION;
  blob.size = sizeof(PersistedConfigBlob);
  blob.sequence = newestSequence + 1U;
  blob.tuning = cfg;
  blob.leds = ledCfg;
  blob.crc32 = persisted_blob_crc(blob);

  bool saved = write_persisted_slot(targetSlot, blob);

  // A long history of development firmware can leave the old per-key namespace
  // crowded. If the compact write fails, reclaim that legacy namespace and retry.
  if (!saved) {
    Serial.println("Compact save failed; clearing legacy Lumina storage and retrying once.");
    clear_legacy_storage();
    saved = write_persisted_slot(targetSlot, blob);
  }

  if (!saved) {
    Serial.printf(
      "Configuration save FAILED after retry: activeStrands=%u RGBscale=[%u,%u,%u]\n",
      ledCfg.activeStrands,
      ledCfg.redScale,
      ledCfg.greenScale,
      ledCfg.blueScale
    );
    return false;
  }

  persistedConfigSequence = blob.sequence;
  configMigrationNeeded = false;
  clear_legacy_storage();

  Serial.printf(
    "Configuration save verified: slot=%s sequence=%lu bytes=%u activeStrands=%u RGBscale=[%u,%u,%u]\n",
    targetSlot,
    (unsigned long)persistedConfigSequence,
    (unsigned)sizeof(PersistedConfigBlob),
    ledCfg.activeStrands,
    ledCfg.redScale,
    ledCfg.greenScale,
    ledCfg.blueScale
  );
  return true;
}

static uint8_t agc_level(float raw, float &peak, float &smooth, float noise_floor, float gain) {
  cfg.attack = clamp_float(cfg.attack, 0.01f, 1.0f);
  cfg.release = clamp_float(cfg.release, 0.01f, 1.0f);
  cfg.peakDecay = clamp_float(cfg.peakDecay, 0.900f, 0.999f);

  raw *= gain;

  if (raw < noise_floor) raw = 0.0f;

  if (raw > peak) {
    peak = raw;
  } else {
    peak = peak * cfg.peakDecay + raw * (1.0f - cfg.peakDecay);
  }

  float min_peak = noise_floor * 5.0f + 1.0f;
  if (peak < min_peak) peak = min_peak;

  float target = (raw / peak) * 255.0f;
  target = clamp_float(target, 0.0f, 255.0f);

  float coeff = (target > smooth) ? cfg.attack : cfg.release;
  smooth += (target - smooth) * coeff;

  return clamp_u8(smooth);
}

static void init_fft_tables() {
  for (int i = 0; i < FFT_SIZE; i++) {
    fftWindow[i] = 0.5f - 0.5f * cosf((2.0f * PI_F * (float)i) / (float)(FFT_SIZE - 1));
  }

  float ratio = FFT_MAX_HZ / FFT_MIN_HZ;

  for (int b = 0; b < FFT_BAND_COUNT; b++) {
    float f0 = FFT_MIN_HZ * powf(ratio, (float)b / (float)FFT_BAND_COUNT);
    float f1 = FFT_MIN_HZ * powf(ratio, (float)(b + 1) / (float)FFT_BAND_COUNT);
    float fc = sqrtf(f0 * f1);

    int startBin = (int)floorf((f0 * (float)FFT_SIZE) / (float)SAMPLE_RATE);
    int endBin = (int)ceilf((f1 * (float)FFT_SIZE) / (float)SAMPLE_RATE);

    if (startBin < 1) startBin = 1;
    if (endBin <= startBin) endBin = startBin + 1;
    if (startBin > FFT_SIZE / 2) startBin = FFT_SIZE / 2;
    if (endBin > FFT_SIZE / 2) endBin = FFT_SIZE / 2;

    fftBandStart[b] = (uint16_t)startBin;
    fftBandEnd[b] = (uint16_t)endBin;
    fftBandCenterHz[b] = fc;
  }

  fftTablesReady = true;
}

static void fft_inplace(float *re, float *im, int n) {
  // Iterative radix-2 Cooley-Tukey FFT. Input size must be a power of two.
  int j = 0;
  for (int i = 1; i < n; i++) {
    int bit = n >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;

    if (i < j) {
      float tr = re[i];
      re[i] = re[j];
      re[j] = tr;

      float ti = im[i];
      im[i] = im[j];
      im[j] = ti;
    }
  }

  for (int len = 2; len <= n; len <<= 1) {
    float angle = -2.0f * PI_F / (float)len;
    float wlen_r = cosf(angle);
    float wlen_i = sinf(angle);
    int half = len >> 1;

    for (int i = 0; i < n; i += len) {
      float wr = 1.0f;
      float wi = 0.0f;

      for (int k = 0; k < half; k++) {
        int even = i + k;
        int odd = even + half;

        float ur = re[even];
        float ui = im[even];
        float vr = re[odd] * wr - im[odd] * wi;
        float vi = re[odd] * wi + im[odd] * wr;

        re[even] = ur + vr;
        im[even] = ui + vi;
        re[odd] = ur - vr;
        im[odd] = ui - vi;

        float next_wr = wr * wlen_r - wi * wlen_i;
        wi = wr * wlen_i + wi * wlen_r;
        wr = next_wr;
      }
    }
  }
}

static void append_fft_samples(const int16_t *samples, int n) {
  if (n <= 0) return;

  if (n >= FFT_SIZE) {
    const int16_t *src = samples + (n - FFT_SIZE);
    memcpy(fftMonoWindow, src, FFT_SIZE * sizeof(int16_t));
    fftFilledSamples = FFT_SIZE;
    fftReady = true;
    return;
  }

  memmove(fftMonoWindow, fftMonoWindow + n, (FFT_SIZE - n) * sizeof(int16_t));
  memcpy(fftMonoWindow + (FFT_SIZE - n), samples, n * sizeof(int16_t));

  uint32_t filled = (uint32_t)fftFilledSamples + (uint32_t)n;
  if (filled > FFT_SIZE) filled = FFT_SIZE;
  fftFilledSamples = (uint16_t)filled;
  fftReady = (fftFilledSamples >= FFT_SIZE);
}

static void record_fft_time(uint32_t startedUs) {
  uint32_t elapsed = micros() - startedUs;
  lastFftUs = elapsed;
  if (elapsed > maxFftUs) maxFftUs = elapsed;
  fftRunCount++;
  if (fftRunCount == 1) avgFftUs = elapsed;
  else avgFftUs = (uint32_t)(((uint64_t)avgFftUs * 31ULL + elapsed) / 32ULL);
}

static float compute_fft_spectral_flux() {
  uint32_t fftStartUs = micros();
  if (!fftTablesReady) init_fft_tables();
  debugFftReady = fftReady ? 1 : 0;

  if (!fftReady) {
    debugSpectralFlux = 0.0f;
    debugLowFlux = 0.0f;
    debugHighFlux = 0.0f;
    debugFftPeakHz = 0.0f;
    record_fft_time(fftStartUs);
    return 0.0f;
  }

  // Remove DC and apply Hann window. Samples are normalized to -1..1 so the
  // spectral-flux tuning values are stable even if ADC gain changes later.
  float mean = 0.0f;
  for (int i = 0; i < FFT_SIZE; i++) {
    mean += (float)fftMonoWindow[i];
  }
  mean /= (float)FFT_SIZE;

  for (int i = 0; i < FFT_SIZE; i++) {
    fftReal[i] = (((float)fftMonoWindow[i] - mean) / 32768.0f) * fftWindow[i];
    fftImag[i] = 0.0f;
  }

  fft_inplace(fftReal, fftImag, FFT_SIZE);

  // Pass 1: compute current log magnitudes for all log-spaced bands.
  // We intentionally do this before updating prevBandLog so the SuperFlux-style
  // neighbor comparison uses only the previous spectrum, not a half-updated mix.
  float peakMag2 = 0.0f;
  int peakBin = 1;

  for (int b = 0; b < FFT_BAND_COUNT; b++) {
    int startBin = fftBandStart[b];
    int endBin = fftBandEnd[b];
    if (endBin < startBin) endBin = startBin;

    float energy = 0.0f;
    int count = 0;

    for (int k = startBin; k <= endBin; k++) {
      float re = fftReal[k];
      float im = fftImag[k];
      float mag2 = re * re + im * im;
      energy += mag2;
      count++;

      if (mag2 > peakMag2) {
        peakMag2 = mag2;
        peakBin = k;
      }
    }

    if (count <= 0) count = 1;

    // Log compression is the key to making the detector work across quiet and
    // loud songs without needing totally different threshold values.
    float bandMag = sqrtf(energy / (float)count);
    fftBandLog[b] = log1pf(bandMag * 45.0f);
  }

  debugFftPeakHz = ((float)peakBin * (float)SAMPLE_RATE) / (float)FFT_SIZE;

  // First full FFT frame only primes the previous spectrum. Without this, the
  // first FFT window after boot/reset looks like one giant fake onset.
  if (!fftBandPrimed) {
    for (int b = 0; b < FFT_BAND_COUNT; b++) {
      prevBandLog[b] = fftBandLog[b];
    }
    fftBandPrimed = true;
    debugSpectralFlux = 0.0f;
    debugLowFlux = 0.0f;
    debugHighFlux = 0.0f;
    record_fft_time(fftStartUs);
    return 0.0f;
  }

  // Pass 2: half-wave rectified spectral flux with simple SuperFlux-style
  // neighbor suppression. A band must exceed the previous frame at that band
  // and its neighbors. This reduces false beats from vibrato and bass pitch
  // slides while still catching kick transients.
  float weightedFlux = 0.0f;
  float weightSum = 0.0f;
  float lowFlux = 0.0f;
  float highFlux = 0.0f;
  int lowCount = 0;
  int highCount = 0;

  for (int b = 0; b < FFT_BAND_COUNT; b++) {
    float previousRef = prevBandLog[b];

    if (b > 0) {
      float neighbor = prevBandLog[b - 1] * 0.94f;
      if (neighbor > previousRef) previousRef = neighbor;
    }

    if (b + 1 < FFT_BAND_COUNT) {
      float neighbor = prevBandLog[b + 1] * 0.94f;
      if (neighbor > previousRef) previousRef = neighbor;
    }

    float delta = fftBandLog[b] - previousRef;
    if (delta < 0.0f) delta = 0.0f;

    float hz = fftBandCenterHz[b];

    // Low bands carry kick/body, mids carry punch/snare, highs carry click.
    // The detector favors low/mid onsets so hats do not become the beat.
    float weight = 0.35f;
    if (hz >= 45.0f && hz < 260.0f) {
      weight = 1.85f;
      lowFlux += delta;
      lowCount++;
    } else if (hz < 1200.0f) {
      weight = 0.90f;
    } else if (hz < 6000.0f) {
      weight = 0.48f;
      highFlux += delta;
      highCount++;
    } else {
      weight = 0.22f;
      highFlux += delta * 0.5f;
      highCount++;
    }

    weightedFlux += delta * weight;
    weightSum += weight;
  }

  for (int b = 0; b < FFT_BAND_COUNT; b++) {
    prevBandLog[b] = fftBandLog[b];
  }

  if (weightSum <= 0.0f) weightSum = 1.0f;
  if (lowCount <= 0) lowCount = 1;
  if (highCount <= 0) highCount = 1;

  debugSpectralFlux = weightedFlux / weightSum;
  debugLowFlux = lowFlux / (float)lowCount;
  debugHighFlux = highFlux / (float)highCount;

  record_fft_time(fftStartUs);
  return debugSpectralFlux;
}

static void update_beat_detector(AudioLedPacket &p, uint32_t now, bool runFft) {
  // Between FFT hops, preserve the current hold/pulse state without repeating
  // expensive spectral analysis or advancing the peak-picker history.
  if (!runFft) {
    p.beat = (now < beatHoldUntilMs) ? 255 : 0;
    p.beatPulse = beat_pulse_counter;
    debugBeatAgeMs = (lastBeatMs > 0) ? (float)(now - lastBeatMs) : 0.0f;
    return;
  }

  float spectralOnset = compute_fft_spectral_flux();

  float logVolume = log1pf((raw_volume * cfg.volumeGain / 32768.0f) * 40.0f);
  float dVolume = logVolume - prevLogVolume;
  if (dVolume < 0.0f) dVolume = 0.0f;
  prevLogVolume = logVolume;

  // Blend broad spectral flux with a dedicated low-band onset term. The
  // broad term catches snare/click transients; the low term improves kick
  // consistency; the small level derivative helps on clean electronic kicks.
  float onset = spectralOnset * 0.72f + debugLowFlux * 0.20f + dVolume * 0.08f;

  if (!beatStatePrimed) {
    onsetMean = onset;
    onsetDev = 0.020f;
    beatStatePrimed = true;
  }

  const float meanCoeff = 0.012f; // similar time constant at the 93.75 Hz, 50%-overlap update rate
  onsetMean += (onset - onsetMean) * meanCoeff;
  onsetDev += (fabsf(onset - onsetMean) - onsetDev) * meanCoeff;

  float threshold = onsetMean + onsetDev * cfg.beatSensitivity + cfg.beatMinOnset;
  if (threshold < cfg.beatMinOnset) threshold = cfg.beatMinOnset;

  bool localPeak = (onsetPrev1 > onsetPrev2) && (onsetPrev1 >= onset);
  float score = onsetPrev1 / (thresholdPrev1 + 0.0001f);

  uint32_t candidateMs = onsetMsPrev1;
  uint16_t safeMaxBpm = cfg.beatMaxBpm < 1 ? 1 : cfg.beatMaxBpm;
  uint16_t safeMinBpm = cfg.beatMinBpm < 1 ? 1 : cfg.beatMinBpm;
  uint32_t minPeriodMs = 60000UL / safeMaxBpm;
  uint32_t maxPeriodMs = 60000UL / safeMinBpm;

  bool gatesOk = (p.volume >= cfg.beatVolMin) && (p.bass >= cfg.beatBassMin);
  bool cooldownOk = (lastBeatMs == 0) || (candidateMs - lastBeatMs >= cfg.beatCooldownMs);
  bool strongEnough = score >= 1.0f;

  bool nearExpected = false;
  if (cfg.beatTempoAssist && beatPeriodMs > 0.0f && lastBeatMs > 0 && candidateMs > lastBeatMs) {
    float expectedMs = (float)lastBeatMs + beatPeriodMs;
    float expectedErrMs = fabsf((float)candidateMs - expectedMs);
    nearExpected = expectedErrMs <= (float)cfg.beatPredictToleranceMs;
  }

  bool assistedPeak = localPeak && nearExpected && score >= cfg.beatPredictStrength;
  bool validPeriodForTempo = false;
  bool beatNow = false;

  if (beatWarmupBlocks > 8 && localPeak && gatesOk && cooldownOk && (strongEnough || assistedPeak)) {
    beatNow = true;
  }

  if (beatNow) {
    if (lastBeatMs > 0 && candidateMs > lastBeatMs) {
      uint32_t delta = candidateMs - lastBeatMs;
      if (beatPeriodMs > 0.0f &&
          delta > (uint32_t)(beatPeriodMs * 1.55f) &&
          delta < (uint32_t)(beatPeriodMs * 2.45f)) {
        delta /= 2;
      }
      if (delta >= minPeriodMs && delta <= maxPeriodMs) {
        validPeriodForTempo = true;
        if (beatPeriodMs <= 0.0f) beatPeriodMs = (float)delta;
        else beatPeriodMs = beatPeriodMs * 0.78f + (float)delta * 0.22f;
      }
    }
    lastBeatMs = candidateMs;
    beatHoldUntilMs = candidateMs + cfg.beatHoldMs;
    beat_pulse_counter++;
  }

  if (!validPeriodForTempo && lastBeatMs > 0 && now - lastBeatMs > maxPeriodMs * 3UL) {
    beatPeriodMs = 0.0f;
  }

  p.beat = (now < beatHoldUntilMs) ? 255 : 0;
  p.beatPulse = beat_pulse_counter;

  debugOnset = onsetPrev1;
  debugThreshold = thresholdPrev1;
  debugScore = score;
  debugBpm = (beatPeriodMs > 1.0f) ? (60000.0f / beatPeriodMs) : 0.0f;
  debugBeatAgeMs = (lastBeatMs > 0) ? (float)(now - lastBeatMs) : 0.0f;
  debugBeatPeriodMs = beatPeriodMs;
  debugCandidate = (localPeak && gatesOk) ? 1 : 0;
  debugTempoAssist = assistedPeak ? 1 : 0;

  onsetPrev2 = onsetPrev1;
  onsetPrev1 = onset;
  thresholdPrev2 = thresholdPrev1;
  thresholdPrev1 = threshold;
  onsetMsPrev2 = onsetMsPrev1;
  onsetMsPrev1 = now;

  if (beatWarmupBlocks < 1000) beatWarmupBlocks++;
}

static AudioLedPacket analyze_audio_block(int frames) {
  float l_sq = 0.0f;
  float r_sq = 0.0f;
  float mono_sq = 0.0f;

  float bass_sq = 0.0f;
  float mid_sq = 0.0f;
  float treble_sq = 0.0f;

  int clip_count = 0;

  for (int i = 0; i < frames; i++) {
    int16_t l = sample32_to_s16(raw_samples[i * 2 + 0]);
    int16_t r = sample32_to_s16(raw_samples[i * 2 + 1]);

    float lf = (float)l;
    float rf = (float)r;
    float mono = (lf + rf) * 0.5f;
    beat_mono_samples[i] = (int16_t)clamp_int((int)mono, -32768, 32767);

    l_sq += lf * lf;
    r_sq += rf * rf;
    mono_sq += mono * mono;

    // Simple crossover-ish filters:
    // bass   ~= 40 Hz to 160 Hz
    // mid    ~= 250 Hz to 4 kHz
    // treble ~= above 4 kHz
    lp40 += A40 * (mono - lp40);
    lp160 += A160 * (mono - lp160);
    lp250 += A250 * (mono - lp250);
    lp4000 += A4000 * (mono - lp4000);

    float bass_band = lp160 - lp40;
    float mid_band = lp4000 - lp250;
    float treble_band = mono - lp4000;

    bass_sq += bass_band * bass_band;
    mid_sq += mid_band * mid_band;
    treble_sq += treble_band * treble_band;

    if (l > 32700 || l < -32700) clip_count++;
    if (r > 32700 || r < -32700) clip_count++;
  }

  raw_left = sqrtf(l_sq / frames);
  raw_right = sqrtf(r_sq / frames);
  raw_volume = sqrtf(mono_sq / frames);
  raw_bass = sqrtf(bass_sq / frames);
  raw_mid = sqrtf(mid_sq / frames);
  raw_treble = sqrtf(treble_sq / frames);

  AudioLedPacket p = {};
  p.magic = PACKET_MAGIC;
  p.version = PACKET_VERSION;
  p.size = sizeof(AudioLedPacket);
  p.sequence = sequence_num;
  p.ms = millis();

  p.volume = agc_level(raw_volume, peak_volume, smooth_volume, cfg.volumeFloor, cfg.volumeGain);
  p.bass = agc_level(raw_bass, peak_bass, smooth_bass, cfg.bassFloor, cfg.bassGain);
  p.mid = agc_level(raw_mid, peak_mid, smooth_mid, cfg.midFloor, cfg.midGain);
  p.treble = agc_level(raw_treble, peak_treble, smooth_treble, cfg.trebleFloor, cfg.trebleGain);
  p.left = agc_level(raw_left, peak_left, smooth_left, cfg.volumeFloor, cfg.lrGain);
  p.right = agc_level(raw_right, peak_right, smooth_right, cfg.volumeFloor, cfg.lrGain);

  p.clip = (clip_count > 255) ? 255 : (uint8_t)clip_count;

  // v22 rave modes use continuously available overall/bass/mid/treble energy
  // instead of requiring a discrete beat decision. Keep the packet fields for
  // protocol stability, but leave them inactive and skip the costly FFT.
  if (ENABLE_BEAT_DETECTOR) {
    append_fft_samples(beat_mono_samples, frames);
    bool runFft = false;
    fftBlockCounter++;
    if (fftReady && fftBlockCounter >= FFT_UPDATE_EVERY_BLOCKS) {
      fftBlockCounter = 0;
      runFft = true;
    }
    update_beat_detector(p, p.ms, runFft);

    float confidence = clamp_float(debugScore / 1.50f, 0.0f, 1.0f);
    p.beatConfidence = (uint8_t)(confidence * 255.0f + 0.5f);
    p.beatBpm = (uint8_t)clamp_int((int)(debugBpm + 0.5f), 0, 255);
    p.beatPhase = 0;
    if (beatPeriodMs > 1.0f && lastBeatMs > 0 && p.ms >= lastBeatMs) {
      float elapsed = fmodf((float)(p.ms - lastBeatMs), beatPeriodMs);
      p.beatPhase = (uint8_t)clamp_int((int)((elapsed / beatPeriodMs) * 255.0f + 0.5f), 0, 255);
    }
  } else {
    p.beat = 0;
    p.beatPulse = 0;
    p.beatConfidence = 0;
    p.beatPhase = 0;
    p.beatBpm = 0;
  }

  p.globalBrightness = ledCfg.globalBrightness;
  p.globalHue = ledCfg.globalHue;
  p.activeStrands = (uint8_t)clamp_int(ledCfg.activeStrands, 1, MAX_STRANDS);
  p.flags = ENABLE_BEAT_DETECTOR ? ((debugCandidate ? 0x01 : 0x00) | (debugTempoAssist ? 0x02 : 0x00)) : 0;
  p.maxLedsPerStrand = (uint16_t)clamp_int(ledCfg.maxLedsPerStrand, 1, 1000);
  p.colorMap = (uint8_t)clamp_int(ledCfg.colorMap, 0, 5);
  p.redScale = ledCfg.redScale;
  p.greenScale = ledCfg.greenScale;
  p.blueScale = ledCfg.blueScale;
  p.gamma10 = (uint8_t)clamp_int(ledCfg.gamma10, 10, 30);
  // beatConfidence, beatPhase, and beatBpm are populated above.
  memcpy(p.strand, ledCfg.strand, sizeof(p.strand));
  p.reserved = 0;

  return p;
}


// ============================================================
// ESP-NOW send callback
// Supports older and newer Arduino-ESP32 cores.
// ============================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void on_espnow_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  espnow_busy = false;
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok++;
  else send_cb_fail++;
}
#else
void on_espnow_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  espnow_busy = false;
  if (status == ESP_NOW_SEND_SUCCESS) send_cb_ok++;
  else send_cb_fail++;
}
#endif

static void maybe_send_espnow(AudioLedPacket &p) {
  uint32_t now = millis();
  bool forced = forceEspNowSend;

  if (!forced && now - last_send_ms < cfg.packetIntervalMs) return;

  if (espnow_busy) {
    if (now - busy_since_ms < 100) {
      send_skipped_busy++;
      return;
    }
    espnow_busy = false;
  }

  espnow_busy = true;
  busy_since_ms = now;
  last_send_ms = now;

  // Sequence numbers now identify transmitted packets, not every audio block.
  // This makes receiver loss statistics meaningful.
  p.sequence = sequence_num;
  esp_err_t err = esp_now_send(BROADCAST_ADDR, (const uint8_t *)&p, sizeof(p));

  if (err == ESP_OK) {
    sequence_num++;
    send_enqueue_ok++;
    forceEspNowSend = false;
  } else {
    espnow_busy = false;
    send_enqueue_err++;
    last_send_err = (uint32_t)err;
  }
}

// ============================================================
// Web handlers
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

  if (now - last_pps_ms >= 1000) {
    uint32_t current = send_enqueue_ok;
    packets_per_sec = current - last_pps_count;
    last_pps_count = current;
    last_pps_ms = now;
  }

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

  if (now - last_pps_ms >= 1000) {
    uint32_t current = send_enqueue_ok;
    packets_per_sec = current - last_pps_count;
    last_pps_count = current;
    last_pps_ms = now;
  }

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

static void setup_web_server() {
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

// ============================================================
// Setup
// ============================================================
static void setup_i2s() {
  I2S.setPins(PIN_BCLK, PIN_LRCK, -1, PIN_DIN, PIN_MCLK);

  bool ok = I2S.begin(
    I2S_MODE_STD,
    SAMPLE_RATE,
    I2S_DATA_BIT_WIDTH_32BIT,
    I2S_SLOT_MODE_STEREO,
    -1,
    I2S_ROLE_MASTER
  );

  Serial.print("I2S begin: ");
  Serial.println(ok ? "OK" : "FAIL");

  if (!ok) {
    while (true) delay(1000);
  }
}

static void setup_wifi_espnow() {
  WiFi.useStaticBuffers(true);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.disconnect(false, false);
  delay(100);

  esp_wifi_set_ps(WIFI_PS_NONE);

  if (USE_STATIC_IP) {
    bool configOk = WiFi.config(local_ip, gateway, subnet, dns1, dns2);
    Serial.print("Static IP config: ");
    Serial.println(configOk ? "OK" : "FAIL");
  }

  Serial.print("Connecting to 2.4 GHz Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t attemptStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    show_connecting_rgb(millis());
    server.handleClient();
    delay(25);

    if (millis() - attemptStart >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("Wi-Fi connection timed out. Retrying...");
      status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
      WiFi.disconnect(false, false);
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      attemptStart = millis();
    }
  }

  active_wifi_channel = WiFi.channel();
  wifiWasConnected = true;
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  show_connected_rgb();

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("Controller page: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("Wi-Fi / ESP-NOW channel: ");
  Serial.println(active_wifi_channel);
  Serial.print("Sender STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("DNS 2: ");
  Serial.println(WiFi.dnsIP(1));

  if (MDNS.begin("lumina")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS page: http://lumina.local");
  } else {
    Serial.println("mDNS startup failed; use the numeric IP address.");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
    while (true) delay(1000);
  }

  esp_now_register_send_cb(on_espnow_sent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
  // Channel 0 tells ESP-NOW to use the station's current router channel.
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;

  esp_err_t add_result = esp_now_add_peer(&peer);
  if (add_result != ESP_OK && add_result != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("Failed to add broadcast peer. err=");
    Serial.println((uint32_t)add_result);
    status_rgb_write(STATUS_RGB_BRIGHTNESS, 0, 0);
    while (true) delay(1000);
  }

  Serial.println("ESP-NOW sender ready");
}

static void maintain_wifi_connection() {
  uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      active_wifi_channel = WiFi.channel();
      show_connected_rgb();
      Serial.print("Wi-Fi reconnected. IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(" channel: ");
      Serial.println(active_wifi_channel);
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("Wi-Fi disconnected; attempting recovery.");
  }

  show_disconnected_rgb(now);
  if (now - lastWifiReconnectAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWifiReconnectAttemptMs = now;
    WiFi.reconnect();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Lumina v20.1: home-Wi-Fi PCM1808 FFT sender + dual-slot CRC saved configuration + simplified UI");
  Serial.print("AudioLedPacket size: ");
  Serial.println(sizeof(AudioLedPacket));

  if (ENABLE_BEAT_DETECTOR) init_fft_tables();
  load_config();
  if (configMigrationNeeded) {
    if (save_config()) {
      configMigrationNeeded = false;
      Serial.println("Saved and verified migrated Lumina settings.");
    } else {
      Serial.println("WARNING: migrated settings could not be verified.");
    }
  }
  reset_agc();
  reset_beat_state();

  setup_status_rgb();
  setup_wifi_espnow();
  setup_web_server();
  setup_i2s();

  Serial.print("Ready. Open http://");
  Serial.println(WiFi.localIP());
  Serial.println("Recorder removed. Commands use immediate delta updates; /api/live is lightweight diagnostics.");
}

void loop() {
  uint32_t loopStartedUs = micros();

  maintain_wifi_connection();

  // Service short browser commands before entering the blocking I2S read.
  server.handleClient();

  size_t got = I2S.readBytes((char *)raw_samples, sizeof(raw_samples));
  int n32 = got / sizeof(int32_t);
  int frames = n32 / 2;

  if (frames > 0) {
    uint32_t analyzeStartedUs = micros();
    AudioLedPacket packet = analyze_audio_block(frames);
    lastAnalyzeUs = micros() - analyzeStartedUs;
    if (lastAnalyzeUs > maxAnalyzeUs) maxAnalyzeUs = lastAnalyzeUs;

    maybe_send_espnow(packet);
    last_packet = packet;
  }

  // Service the web server again immediately after audio/FFT work.
  server.handleClient();

  uint32_t now = millis();
  static uint32_t last_print_ms = 0;
  if (now - last_print_ms > 1000) {
    last_print_ms = now;
    Serial.printf(
      "vol=%3u bass=%3u mid=%3u treb=%3u beat=%3u bpm=%5.1f pps=%lu rssi=%ddBm ch=%u fft=%luus(avg%lu) analyze=%luus http=%luus heap=%lu\n",
      last_packet.volume,
      last_packet.bass,
      last_packet.mid,
      last_packet.treble,
      last_packet.beat,
      debugBpm,
      (unsigned long)packets_per_sec,
      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127,
      active_wifi_channel,
      (unsigned long)lastFftUs,
      (unsigned long)avgFftUs,
      (unsigned long)lastAnalyzeUs,
      (unsigned long)lastHttpUs,
      (unsigned long)ESP.getFreeHeap()
    );
  }

  lastLoopUs = micros() - loopStartedUs;
  if (lastLoopUs > maxLoopUs) maxLoopUs = lastLoopUs;
  delay(0); // yield to Wi-Fi/TCP/IP tasks
}

