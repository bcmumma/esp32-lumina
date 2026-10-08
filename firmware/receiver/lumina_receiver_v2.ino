// Lumina v22 receiver: automatic channel discovery, four strands, rave/electronic effects, VU tuning tools
// Starts with your known-working setup by default:
//   Strand 1: GPIO33, 10 LEDs, NEO_RGB + NEO_KHZ800
//   Strand 2: GPIO25
//   Strand 3: GPIO26
//   Strand 4: GPIO27
// Then accepts pin/count/type updates from the sender webpage Hardware tab.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <Adafruit_NeoPixel.h>

// ============================================================
// ESP-NOW channel discovery
// ============================================================
// In home-Wi-Fi mode, the sender must use the router's 2.4 GHz channel.
// This receiver automatically scans channels until it finds valid Lumina
// packets, locks to that channel, and resumes scanning if packets disappear.
static constexpr bool AUTO_CHANNEL_SCAN = true;
static constexpr uint8_t FIRST_WIFI_CHANNEL = 1;
static constexpr uint8_t LAST_WIFI_CHANNEL = 11; // US 2.4 GHz channels
static constexpr uint32_t CHANNEL_DWELL_MS = 220;
static constexpr uint32_t CHANNEL_RESCAN_TIMEOUT_MS = 3000;

volatile uint8_t currentWifiChannel = FIRST_WIFI_CHANNEL;
volatile uint8_t lockedWifiChannel = 0;
volatile bool wifiChannelLocked = false;
uint32_t lastChannelHopMs = 0;
uint8_t lastPrintedLockedChannel = 0;

// Packet supports up to four LED strands. The sender webpage Hardware tab
// provides per-strand pin/count/type. A hard cap remains here for safety.
static constexpr uint8_t MAX_STRANDS = 4;
static constexpr uint16_t SAFETY_MAX_LEDS_PER_STRAND = 1000;

// Debug options
static constexpr bool BOOT_TEST = true;
static constexpr bool FORCE_VISIBLE_IF_BAD_SETTINGS = false; // normal operation: Off/disabled/white must be honored
static constexpr uint8_t SAFE_BRIGHTNESS = 220;
static constexpr uint32_t FRAME_INTERVAL_MS = 16;   // up to ~60 FPS; LED show time may lower this
static constexpr uint32_t PACKET_TIMEOUT_MS = 1500;

// ============================================================
// Packet format - must match sender.
// Packet layout must exactly match the v22 sender. Packet version 12.
// ============================================================
static constexpr uint16_t PACKET_MAGIC = 0xA17D;
static constexpr uint8_t PACKET_VERSION = 12;

enum LedType : uint8_t {
  LED_TYPE_WS2815_WS2812_RGB_800 = 0,
  LED_TYPE_WS2811_RGB_800 = 1,
  LED_TYPE_WS2811_GRB_800 = 2,
  LED_TYPE_WS2811_RGB_400 = 3,
  LED_TYPE_SK6812_RGB_GRB = 4,
  LED_TYPE_SK6812_RGBW_GRBW = 5,
  LED_TYPE_SK6812_RGBW_RGBW = 6
};

enum ColorMap : uint8_t {
  COLOR_MAP_RGB = 0,
  COLOR_MAP_RBG = 1,
  COLOR_MAP_GRB = 2,
  COLOR_MAP_GBR = 3,
  COLOR_MAP_BRG = 4,
  COLOR_MAP_BGR = 5
};

// Declared before any function definitions so the Arduino .ino preprocessor
// can generate valid prototypes for VU helper functions.
enum VuSource : uint8_t {
  VU_SOURCE_OVERALL = 0,
  VU_SOURCE_BASS = 1,
  VU_SOURCE_MID = 2,
  VU_SOURCE_TREBLE = 3
};

struct __attribute__((packed)) StrandSettings {
  uint8_t enabled;
  uint8_t mode;
  uint8_t brightness;
  uint8_t hue;
  uint8_t saturation;
  uint8_t speed;
  uint8_t density;
  uint8_t sensitivity;
  uint8_t strobe;
  uint8_t twinkle;
  uint8_t direction;
  uint8_t pin;
  uint8_t ledType;
  uint8_t reserved8;
  uint16_t ledCount;
  uint16_t reserved;
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
  uint8_t beat;
  uint8_t clip;
  uint8_t beatPulse;
  uint8_t globalBrightness;
  uint8_t globalHue;
  uint8_t activeStrands;
  uint8_t flags; // bit0=onset candidate, bit1=tempo-assisted candidate
  uint16_t maxLedsPerStrand;

  uint8_t colorMap;
  uint8_t redScale;
  uint8_t greenScale;
  uint8_t blueScale;
  uint8_t gamma10;
  uint8_t beatConfidence;
  uint8_t beatPhase;
  uint8_t beatBpm;

  StrandSettings strand[MAX_STRANDS];
  uint16_t reserved;
};

static_assert(sizeof(AudioLedPacket) < 250, "AudioLedPacket must fit in an ESP-NOW v1 payload");

// ============================================================
// State
// ============================================================
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
AudioLedPacket latestPacket = {};
uint8_t latestMac[6] = {};
volatile bool havePacket = false;
volatile uint32_t goodPackets = 0;
volatile uint32_t badPackets = 0;
volatile uint32_t packetGeneration = 0;

uint32_t renderedFrames = 0;
uint32_t renderFps = 0;
uint32_t lastFpsMs = 0;
uint32_t renderUs = 0;
uint32_t renderUsMax = 0;
uint32_t showUs = 0;
uint32_t showUsMax = 0;

uint16_t lastSequence = 0;
uint32_t lostPackets = 0;
uint32_t lastPacketMs = 0;
uint8_t lastBeatPulse = 0;
uint8_t beatFlash = 0;

// Runtime LED hardware. Each strand is allocated/reallocated only when the
// webpage hardware settings change. Default data pins are GPIO33, GPIO25, GPIO26, and GPIO27.
Adafruit_NeoPixel *strips[MAX_STRANDS] = { nullptr, nullptr, nullptr, nullptr };
bool configured[MAX_STRANDS] = { false, false, false, false };
uint8_t configuredPin[MAX_STRANDS] = { 255, 255, 255, 255 };
uint16_t configuredCount[MAX_STRANDS] = { 0, 0, 0, 0 };
uint8_t configuredType[MAX_STRANDS] = { LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800, LED_TYPE_WS2815_WS2812_RGB_800 };

static constexpr uint8_t DEFAULT_LED_PINS[MAX_STRANDS] = { 33, 25, 26, 27 };
static constexpr uint16_t DEFAULT_LED_COUNTS[MAX_STRANDS] = { 10, 0, 0, 0 };
static constexpr uint8_t DEFAULT_TYPE[MAX_STRANDS] = {
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800,
  LED_TYPE_WS2815_WS2812_RGB_800
};

// Current color calibration received from sender.
uint8_t activeColorMap = COLOR_MAP_RGB;
uint8_t activeRedScale = 255;
uint8_t activeGreenScale = 255;
uint8_t activeBlueScale = 255;
uint8_t activeGamma10 = 10;
uint8_t calibrationLutR[256] = {};
uint8_t calibrationLutG[256] = {};
uint8_t calibrationLutB[256] = {};

struct StrandRuntime {
  uint16_t pos;
  uint16_t aux;
  uint8_t flash;
  uint8_t sparkle;
  uint8_t meterPeak;
  uint8_t prevEnergy;
  uint8_t prevTreble;
  uint8_t phase;
  uint32_t lastTriggerMs;
};

StrandRuntime rt[MAX_STRANDS];

// ============================================================
// Color helpers
// ============================================================
struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static uint8_t qadd8(uint8_t a, uint8_t b) {
  uint16_t s = (uint16_t)a + (uint16_t)b;
  return s > 255 ? 255 : (uint8_t)s;
}

static uint8_t scale8(uint8_t v, uint8_t scale) {
  return ((uint16_t)v * (uint16_t)scale) >> 8;
}

static uint8_t scale8_video(uint8_t v, uint8_t scale) {
  uint16_t out = ((uint16_t)v * (uint16_t)scale) >> 8;
  if (v && scale) out++;
  return out > 255 ? 255 : (uint8_t)out;
}

static Rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0) return {v, v, v};

  uint8_t region = h / 43;
  uint8_t remainder = (h - region * 43) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
  }
}

static Rgb scale_color(Rgb c, uint8_t scale) {
  c.r = scale8_video(c.r, scale);
  c.g = scale8_video(c.g, scale);
  c.b = scale8_video(c.b, scale);
  return c;
}

static Rgb add_color(Rgb a, Rgb b) {
  return {qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)};
}

static uint8_t build_corrected_value(uint8_t v, uint8_t scale, uint8_t gamma10) {
  float x = (float)v / 255.0f;
  float gamma = (float)gamma10 / 10.0f;
  if (gamma < 0.5f) gamma = 1.0f;
  float y = powf(x, gamma) * 255.0f;
  y = y * ((float)scale / 255.0f);
  if (y < 0.0f) return 0;
  if (y > 255.0f) return 255;
  return (uint8_t)(y + 0.5f);
}

static void rebuild_color_luts() {
  for (int i = 0; i < 256; i++) {
    calibrationLutR[i] = build_corrected_value((uint8_t)i, activeRedScale, activeGamma10);
    calibrationLutG[i] = build_corrected_value((uint8_t)i, activeGreenScale, activeGamma10);
    calibrationLutB[i] = build_corrected_value((uint8_t)i, activeBlueScale, activeGamma10);
  }
}

static void update_color_calibration(const AudioLedPacket &p) {
  uint8_t newMap = (p.colorMap <= 5) ? p.colorMap : COLOR_MAP_RGB;
  uint8_t newGamma = (p.gamma10 >= 10 && p.gamma10 <= 30) ? p.gamma10 : 10;
  bool changed = newMap != activeColorMap ||
                 p.redScale != activeRedScale ||
                 p.greenScale != activeGreenScale ||
                 p.blueScale != activeBlueScale ||
                 newGamma != activeGamma10;

  activeColorMap = newMap;
  activeRedScale = p.redScale;
  activeGreenScale = p.greenScale;
  activeBlueScale = p.blueScale;
  activeGamma10 = newGamma;

  if (changed) rebuild_color_luts();
}

static Rgb apply_color_calibration(Rgb c) {
  Rgb scaled = {
    calibrationLutR[c.r],
    calibrationLutG[c.g],
    calibrationLutB[c.b]
  };

  switch (activeColorMap) {
    case COLOR_MAP_RBG: return {scaled.r, scaled.b, scaled.g};
    case COLOR_MAP_GRB: return {scaled.g, scaled.r, scaled.b};
    case COLOR_MAP_GBR: return {scaled.g, scaled.b, scaled.r};
    case COLOR_MAP_BRG: return {scaled.b, scaled.r, scaled.g};
    case COLOR_MAP_BGR: return {scaled.b, scaled.g, scaled.r};
    case COLOR_MAP_RGB:
    default: return scaled;
  }
}

static neoPixelType pixel_type_from_id(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return NEO_RGB + NEO_KHZ800;
    case LED_TYPE_WS2811_GRB_800: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_WS2811_RGB_400: return NEO_RGB + NEO_KHZ400;
    case LED_TYPE_SK6812_RGB_GRB: return NEO_GRB + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_GRBW: return NEO_GRBW + NEO_KHZ800;
    case LED_TYPE_SK6812_RGBW_RGBW: return NEO_RGBW + NEO_KHZ800;
    case LED_TYPE_WS2815_WS2812_RGB_800:
    default: return NEO_RGB + NEO_KHZ800;
  }
}

static const char *pixel_type_name(uint8_t ledType) {
  switch (ledType) {
    case LED_TYPE_WS2811_RGB_800: return "WS2811 RGB 800k";
    case LED_TYPE_WS2811_GRB_800: return "WS2811 GRB 800k";
    case LED_TYPE_WS2811_RGB_400: return "WS2811 RGB 400k";
    case LED_TYPE_SK6812_RGB_GRB: return "SK6812 RGB GRB";
    case LED_TYPE_SK6812_RGBW_GRBW: return "SK6812 RGBW GRBW";
    case LED_TYPE_SK6812_RGBW_RGBW: return "SK6812 RGBW RGBW";
    case LED_TYPE_WS2815_WS2812_RGB_800:
    default: return "WS2815/WS2812 RGB 800k";
  }
}

static bool usable_pin(uint8_t pin) {
  // Keep this permissive because ESP32 board variants expose different pins.
  // Avoid classic UART0 pins 1/3 and input-only pins 34-39 for LED data.
  if (pin == 1 || pin == 3) return false;
  if (pin >= 34 && pin <= 39) return false;
  return pin <= 48;
}

static void disable_strand(uint8_t s) {
  if (s >= MAX_STRANDS) return;
  if (strips[s]) {
    strips[s]->clear();
    strips[s]->show();
    delete strips[s];
    strips[s] = nullptr;
  }
  configured[s] = false;
  configuredCount[s] = 0;
}

static void configure_strand(uint8_t s, uint8_t pin, uint16_t count, uint8_t ledType) {
  if (s >= MAX_STRANDS) return;
  if (count > SAFETY_MAX_LEDS_PER_STRAND) count = SAFETY_MAX_LEDS_PER_STRAND;
  if (ledType > LED_TYPE_SK6812_RGBW_RGBW) ledType = LED_TYPE_WS2815_WS2812_RGB_800;

  if (count == 0 || !usable_pin(pin)) {
    if (configured[s]) Serial.printf("Strand %u disabled: pin=%u count=%u\n", s + 1, pin, count);
    disable_strand(s);
    configuredPin[s] = pin;
    configuredType[s] = ledType;
    return;
  }

  if (configured[s] && strips[s] && configuredPin[s] == pin && configuredCount[s] == count && configuredType[s] == ledType) {
    return;
  }

  disable_strand(s);

  strips[s] = new Adafruit_NeoPixel(count, pin, pixel_type_from_id(ledType));
  if (!strips[s]) {
    Serial.printf("Strand %u allocation failed: pin=%u count=%u\n", s + 1, pin, count);
    configured[s] = false;
    configuredCount[s] = 0;
    return;
  }

  strips[s]->begin();
  strips[s]->setBrightness(255);
  strips[s]->clear();
  strips[s]->show();

  configured[s] = true;
  configuredPin[s] = pin;
  configuredCount[s] = count;
  configuredType[s] = ledType;
  rt[s] = {};

  Serial.printf("Strand %u configured: pin=%u count=%u type=%s\n", s + 1, pin, count, pixel_type_name(ledType));
}

static void apply_hardware_from_packet(const AudioLedPacket &p) {
  uint8_t active = p.activeStrands;
  if (active < 1) active = 1;
  if (active > MAX_STRANDS) active = MAX_STRANDS;

  uint16_t packetCap = p.maxLedsPerStrand;
  if (packetCap < 1) packetCap = 1;
  if (packetCap > SAFETY_MAX_LEDS_PER_STRAND) packetCap = SAFETY_MAX_LEDS_PER_STRAND;

  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    uint16_t wantedCount = p.strand[s].ledCount;
    if (wantedCount > packetCap) wantedCount = packetCap;

    if (s >= active || !p.strand[s].enabled || wantedCount == 0) {
      configure_strand(s, p.strand[s].pin, 0, p.strand[s].ledType);
    } else {
      configure_strand(s, p.strand[s].pin, wantedCount, p.strand[s].ledType);
    }
  }
}

static uint32_t make_color(uint8_t s, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return 0;
  Rgb native = apply_color_calibration(c);
  return strips[s]->Color(native.r, native.g, native.b);
}

static Rgb read_color(uint8_t s, uint16_t i) {
  if (s >= MAX_STRANDS || !strips[s] || i >= configuredCount[s]) return {0, 0, 0};
  uint32_t c = strips[s]->getPixelColor(i);
  return {
    (uint8_t)((c >> 16) & 0xFF),
    (uint8_t)((c >> 8) & 0xFF),
    (uint8_t)(c & 0xFF)
  };
}

static uint16_t count_for(uint8_t s) {
  if (s >= MAX_STRANDS) return 0;
  if (!configured[s] || !strips[s]) return 0;
  return configuredCount[s];
}

static void set_pixel_native(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  strips[s]->setPixelColor(i, strips[s]->Color(c.r, c.g, c.b));
}

static void set_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  set_pixel_native(s, i, apply_color_calibration(c));
}

static void add_pixel(uint8_t s, uint16_t i, Rgb c) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  if (i >= count_for(s)) return;
  Rgb oldNative = read_color(s, i);
  Rgb addNative = apply_color_calibration(c);
  set_pixel_native(s, i, add_color(oldNative, addNative));
}

static void fill_strand(uint8_t s, Rgb c) {
  uint16_t n = count_for(s);
  for (uint16_t i = 0; i < n; i++) set_pixel(s, i, c);
}

static void clear_strand(uint8_t s) {
  if (s >= MAX_STRANDS || !strips[s]) return;
  strips[s]->clear();
}

static void fade_strand(uint8_t s, uint8_t amount) {
  uint16_t n = count_for(s);
  uint8_t keep = 255 - amount;
  for (uint16_t i = 0; i < n; i++) {
    set_pixel_native(s, i, scale_color(read_color(s, i), keep));
  }
}

static void show_all() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    if (configured[s] && strips[s] && count_for(s) > 0) strips[s]->show();
  }
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
static void render_off(uint8_t s) {
  clear_strand(s);
}

static void render_solid(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t b = combined_brightness(p, st);
  fill_strand(s, strand_color(p, st, 0, b));
}

static void render_debug_chase(uint8_t s, const AudioLedPacket &p) {
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

static void render_space_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint8_t audio = scaled_audio(p.volume, st.sensitivity);
  uint8_t b = scale8_video(combined_brightness(p, st), max<uint8_t>(30, audio));
  fill_strand(s, strand_color(p, st, 0, b));
}

static void render_beat_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  if (newBeat) rt[s].flash = 255;
  fade_strand(s, st.strobe > 0 ? st.strobe : 80);
  if (rt[s].flash > 10) {
    fill_strand(s, strand_color(p, st, 0, combined_brightness(p, st)));
    rt[s].flash = scale8(rt[s].flash, 150);
  }
}

static void render_bass_comet(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_twinkle(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
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

static void render_spectrum(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_band_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, VuSource source) {
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

static void render_vu(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  render_band_vu(s, p, st, VU_SOURCE_OVERALL);
}

static void render_beat_tracker(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
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

  if (newBeat) rt[s].flash = 255;
  if (rt[s].flash > 2) {
    uint8_t flashBrightness = scale8_video(brightness, rt[s].flash);
    Rgb flashColor = strand_color(p, st, 0, flashBrightness);
    for (uint16_t i = 0; i < n; i++) add_pixel(s, i, flashColor);

    // Higher Strobe/decay means a longer visible beat flash.
    uint8_t keep = (uint8_t)(120U + ((uint16_t)st.strobe * 115U) / 255U);
    rt[s].flash = scale8(rt[s].flash, keep);
  }
}

static void render_glitter_kick(uint8_t s, const AudioLedPacket &p, const StrandSettings &st, bool newBeat) {
  fade_strand(s, 40);
  uint16_t n = count_for(s);
  if (n == 0) return;

  uint8_t sparkles = st.density / 28;
  if (newBeat) sparkles += 16;

  for (uint8_t k = 0; k < sparkles; k++) {
    add_pixel(s, random(n), strand_color(p, st, random(0, 180), combined_brightness(p, st)));
  }
}

static void render_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_aurora_drift(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_starfield(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_neon_scanner(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_laser_sweep(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_rave_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_bass_cannons(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_dnb_chopper(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_acid_chase(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_portal_pulse(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_spectrum_sparks(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_hyper_strobe(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  uint8_t energy = energy_mix(p);
  int16_t delta = (int16_t)energy - (int16_t)rt[s].prevEnergy;
  rt[s].prevEnergy = energy;
  uint8_t threshold = (uint8_t)map(st.strobe, 0, 255, 72, 14);
  uint32_t now = millis();
  if (delta > threshold && now - rt[s].lastTriggerMs > 35U) {
    rt[s].flash = (uint8_t)min<int>(255, 120 + delta * 3);
    rt[s].lastTriggerMs = now;
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

static void render_cyber_tunnel(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_white_lightning(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
  uint16_t n = count_for(s);
  if (n == 0) return;
  fade_strand(s, (uint8_t)map(st.strobe, 0, 255, 120, 34));
  int16_t delta = (int16_t)p.treble - (int16_t)rt[s].prevTreble;
  rt[s].prevTreble = p.treble;
  uint8_t threshold = (uint8_t)map(st.sensitivity, 0, 255, 65, 12);
  uint32_t now = millis();
  if (delta > threshold && now - rt[s].lastTriggerMs > 45U) {
    rt[s].flash = (uint8_t)min<int>(255, 135 + delta * 3);
    rt[s].lastTriggerMs = now;
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

static void render_dnb_split(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_rave_tiles(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_pulse_train(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_void_bloom(uint8_t s, const AudioLedPacket &p, const StrandSettings &st) {
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

static void render_strand(uint8_t s, const AudioLedPacket &p, bool newBeat) {
  if (s >= MAX_STRANDS || !configured[s] || !strips[s] || count_for(s) == 0) return;

  StrandSettings st = p.strand[s];

  // Hardware is configured separately by apply_hardware_from_packet().
  // Rendering uses the currently configured local strip.
  st.ledCount = count_for(s);

  if (FORCE_VISIBLE_IF_BAD_SETTINGS) {
    if (st.enabled == 0) st.enabled = 1;
    if (st.brightness == 0) st.brightness = SAFE_BRIGHTNESS;
    // Saturation 0 is intentional white. Never replace it with 255; the old
    // failsafe converted Test White into hue-0 red.
    if (st.mode == 0) st.mode = 10;
  }

  if (!st.enabled) {
    render_off(s);
    return;
  }

  switch (st.mode) {
    case 0: render_off(s); break;
    case 1: render_space_pulse(s, p, st); break;
    case 2: render_beat_strobe(s, p, st, newBeat); break;
    case 3: render_bass_comet(s, p, st); break;
    case 4: render_twinkle(s, p, st, newBeat); break;
    case 5: render_spectrum(s, p, st); break;
    case 6: render_vu(s, p, st); break;
    case 7: render_glitter_kick(s, p, st, newBeat); break;
    case 8: render_scanner(s, p, st); break;
    case 9: render_solid(s, p, st); break;
    case 10: render_debug_chase(s, p); break;
    case 11: render_band_vu(s, p, st, VU_SOURCE_BASS); break;
    case 12: render_band_vu(s, p, st, VU_SOURCE_MID); break;
    case 13: render_band_vu(s, p, st, VU_SOURCE_TREBLE); break;
    case 14: render_beat_tracker(s, p, st, newBeat); break;
    case 15: render_aurora_drift(s, p, st); break;
    case 16: render_starfield(s, p, st); break;
    case 17: render_neon_scanner(s, p, st); break;
    case 18: render_laser_sweep(s, p, st); break;
    case 19: render_rave_chase(s, p, st); break;
    case 20: render_bass_cannons(s, p, st); break;
    case 21: render_dnb_chopper(s, p, st); break;
    case 22: render_acid_chase(s, p, st); break;
    case 23: render_portal_pulse(s, p, st); break;
    case 24: render_spectrum_sparks(s, p, st); break;
    case 25: render_hyper_strobe(s, p, st); break;
    case 26: render_cyber_tunnel(s, p, st); break;
    case 27: render_white_lightning(s, p, st); break;
    case 28: render_dnb_split(s, p, st); break;
    case 29: render_rave_tiles(s, p, st); break;
    case 30: render_pulse_train(s, p, st); break;
    case 31: render_void_bloom(s, p, st); break;
    default: render_solid(s, p, st); break;
  }
}

// ============================================================
// ESP-NOW
// ============================================================
static void copy_packet_from_callback(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  AudioLedPacket p = {};
  memcpy(&p, data, sizeof(p));

  if (p.magic != PACKET_MAGIC || p.version != PACKET_VERSION || p.size != sizeof(AudioLedPacket)) {
    badPackets++;
    return;
  }

  if (AUTO_CHANNEL_SCAN && !wifiChannelLocked) {
    lockedWifiChannel = currentWifiChannel;
    wifiChannelLocked = true;
  }

  portENTER_CRITICAL(&packetMux);
  latestPacket = p;
  memcpy(latestMac, mac, 6);
  havePacket = true;
  goodPackets++;
  packetGeneration++;
  portEXIT_CRITICAL(&packetMux);
}

#if ESP_IDF_VERSION_MAJOR >= 5
void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  copy_packet_from_callback(info->src_addr, data, len);
}
#else
void on_data_recv(const uint8_t *mac, const uint8_t *data, int len) {
  copy_packet_from_callback(mac, data, len);
}
#endif

static void set_receiver_channel(uint8_t channel) {
  if (channel < FIRST_WIFI_CHANNEL || channel > LAST_WIFI_CHANNEL) return;
  esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    currentWifiChannel = channel;
  } else {
    Serial.print("Failed to set Wi-Fi channel ");
    Serial.print(channel);
    Serial.print(" err=");
    Serial.println((uint32_t)err);
  }
}

static void setup_wifi_espnow() {
  WiFi.useStaticBuffers(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  set_receiver_channel(FIRST_WIFI_CHANNEL);

  Serial.print("LED receiver MAC: ");
  Serial.println(WiFi.macAddress());
  if (AUTO_CHANNEL_SCAN) {
    Serial.print("Scanning ESP-NOW channels ");
    Serial.print(FIRST_WIFI_CHANNEL);
    Serial.print(" through ");
    Serial.println(LAST_WIFI_CHANNEL);
  } else {
    Serial.print("Listening on fixed Wi-Fi channel: ");
    Serial.println((uint8_t)currentWifiChannel);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }

  if (esp_now_register_recv_cb(on_data_recv) != ESP_OK) {
    Serial.println("ESP-NOW receive callback registration failed");
    while (true) delay(1000);
  }
}

static void maintain_channel_discovery(uint32_t now) {
  if (!AUTO_CHANNEL_SCAN) return;

  if (wifiChannelLocked) {
    if (lockedWifiChannel != 0 && currentWifiChannel != lockedWifiChannel) {
      set_receiver_channel(lockedWifiChannel);
    }

    if (lastPacketMs > 0 && now - lastPacketMs > CHANNEL_RESCAN_TIMEOUT_MS) {
      Serial.println("Lumina packets timed out; resuming channel scan.");
      wifiChannelLocked = false;
      lockedWifiChannel = 0;
      lastChannelHopMs = 0;
    }
    return;
  }

  if (now - lastChannelHopMs < CHANNEL_DWELL_MS) return;
  lastChannelHopMs = now;

  uint8_t next = (uint8_t)currentWifiChannel + 1;
  if (next > LAST_WIFI_CHANNEL) next = FIRST_WIFI_CHANNEL;
  set_receiver_channel(next);
}

// ============================================================
// Setup / loop
// ============================================================
static void setup_leds() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    rt[s] = {};
    configure_strand(s, DEFAULT_LED_PINS[s], DEFAULT_LED_COUNTS[s], DEFAULT_TYPE[s]);
  }
}

static void boot_test() {
  if (!BOOT_TEST) return;
  if (!configured[0] || !strips[0]) {
    Serial.println("Boot LED test skipped: Strand 1 is not configured.");
    return;
  }

  Serial.println("Boot LED test: exact known-working command on Strand 1");
  Serial.println("This should light Strand 1 on GPIO33 / 10 LEDs using RGB 800kHz.");

  for (uint16_t i = 0; i < configuredCount[0]; i++) {
    strips[0]->setPixelColor(i, strips[0]->Color(0, 255, 0));
  }
  strips[0]->show();
  delay(1000);

  for (uint16_t i = 0; i < configuredCount[0]; i++) {
    strips[0]->setPixelColor(i, strips[0]->Color(255, 255, 255));
  }
  strips[0]->show();
  delay(600);

  strips[0]->clear();
  strips[0]->show();
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Lumina LED Receiver V22 - rave/electronic effects and VU tuning tools");
  Serial.print("Expected packet size: ");
  Serial.println(sizeof(AudioLedPacket));
  Serial.println("Default data pins: Strand 1=GPIO33, Strand 2=GPIO25, Strand 3=GPIO26, Strand 4=GPIO27.");
  Serial.println("Strand 1 starts with 10 LEDs; set the other LED counts and enable states from the Hardware tab.");

  randomSeed((uint32_t)esp_random());
  rebuild_color_luts();
  setup_leds();
  boot_test();
  setup_wifi_espnow();

  Serial.println("Waiting for ESP-NOW packets...");
}

void loop() {
  static AudioLedPacket currentPacket = {};
  static bool haveCurrentPacket = false;
  static uint32_t seenGeneration = 0;
  static bool pendingBeat = false;
  static uint32_t lastFrameMs = 0;
  static uint32_t lastFadeMs = 0;

  uint32_t generation = 0;
  uint32_t good = 0;
  uint32_t bad = 0;
  bool newPacket = false;

  portENTER_CRITICAL(&packetMux);
  generation = packetGeneration;
  good = goodPackets;
  bad = badPackets;
  if (havePacket && generation != seenGeneration) {
    currentPacket = latestPacket;
    newPacket = true;
  }
  portEXIT_CRITICAL(&packetMux);

  uint32_t now = millis();
  maintain_channel_discovery(now);

  if (wifiChannelLocked && lockedWifiChannel != lastPrintedLockedChannel) {
    lastPrintedLockedChannel = lockedWifiChannel;
    Serial.print("Locked to Lumina sender on Wi-Fi channel ");
    Serial.println(lastPrintedLockedChannel);
  }

  if (newPacket) {
    seenGeneration = generation;
    haveCurrentPacket = true;

    if (lastSequence != 0) {
      uint16_t expected = (uint16_t)(lastSequence + 1);
      if (currentPacket.sequence != expected) {
        uint16_t missing = (uint16_t)(currentPacket.sequence - expected);
        lostPackets += missing;
      }
    }
    lastSequence = currentPacket.sequence;
    lastPacketMs = now;

    // Hardware and gamma/color lookup tables are touched only when a new
    // packet arrives, never on every render frame.
    apply_hardware_from_packet(currentPacket);
    update_color_calibration(currentPacket);

    if (currentPacket.beatPulse != lastBeatPulse) {
      lastBeatPulse = currentPacket.beatPulse;
      beatFlash = 255;
      pendingBeat = true;
    }
  }

  bool packetTimedOut = haveCurrentPacket && lastPacketMs > 0 && (now - lastPacketMs > PACKET_TIMEOUT_MS);

  if (haveCurrentPacket && !packetTimedOut && now - lastFrameMs >= FRAME_INTERVAL_MS) {
    lastFrameMs = now;
    uint32_t renderStartUs = micros();

    for (uint8_t s = 0; s < MAX_STRANDS; s++) {
      render_strand(s, currentPacket, pendingBeat);
    }
    pendingBeat = false;
    renderUs = micros() - renderStartUs;
    if (renderUs > renderUsMax) renderUsMax = renderUs;

    uint32_t showStartUs = micros();
    show_all();
    showUs = micros() - showStartUs;
    if (showUs > showUsMax) showUsMax = showUs;

    renderedFrames++;
    if (beatFlash > 0) beatFlash = scale8(beatFlash, 185);
  }

  // If packets stop, fade at the normal frame rate rather than repeatedly
  // hammering every strand as fast as loop() can run.
  if (packetTimedOut && now - lastFadeMs >= FRAME_INTERVAL_MS) {
    lastFadeMs = now;
    for (uint8_t s = 0; s < MAX_STRANDS; s++) fade_strand(s, 20);
    show_all();
  }

  if (now - lastFpsMs >= 1000) {
    renderFps = renderedFrames;
    renderedFrames = 0;
    lastFpsMs = now;
  }

  static uint32_t lastPrint = 0;
  if (now - lastPrint > 1000) {
    lastPrint = now;
    if (haveCurrentPacket) {
      const AudioLedPacket &p = currentPacket;
      Serial.printf(
        "rx good=%lu bad=%lu lost=%lu seq=%u age=%lums ch=%u locked=%u fps=%lu render=%luus show=%luus vol=%3u bass=%3u mid=%3u treb=%3u beat=%3u pulse=%3u conf=%3u bpm=%3u gB=%3u modes=[%u,%u,%u,%u] br=[%u,%u,%u,%u] en=[%u,%u,%u,%u] pins=[%u,%u,%u,%u] counts=[%u,%u,%u,%u] cmap=%u rgbScale=[%u,%u,%u] gamma10=%u\n",
        (unsigned long)good,
        (unsigned long)bad,
        (unsigned long)lostPackets,
        p.sequence,
        (unsigned long)(now - lastPacketMs),
        (unsigned int)currentWifiChannel,
        wifiChannelLocked ? 1U : 0U,
        (unsigned long)renderFps,
        (unsigned long)renderUs,
        (unsigned long)showUs,
        p.volume,
        p.bass,
        p.mid,
        p.treble,
        p.beat,
        p.beatPulse,
        p.beatConfidence,
        p.beatBpm,
        p.globalBrightness,
        p.strand[0].mode, p.strand[1].mode, p.strand[2].mode, p.strand[3].mode,
        p.strand[0].brightness, p.strand[1].brightness, p.strand[2].brightness, p.strand[3].brightness,
        p.strand[0].enabled, p.strand[1].enabled, p.strand[2].enabled, p.strand[3].enabled,
        configuredPin[0], configuredPin[1], configuredPin[2], configuredPin[3],
        configuredCount[0], configuredCount[1], configuredCount[2], configuredCount[3],
        p.colorMap,
        p.redScale,
        p.greenScale,
        p.blueScale,
        p.gamma10
      );
    } else {
      Serial.printf("No packets yet. Scanning channel %u. Boot test should already have lit default GPIO33/10 LEDs.\n", (unsigned int)currentWifiChannel);
    }
  }

  yield();
  delay(1);
}
