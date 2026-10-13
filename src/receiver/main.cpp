// Lumina v22 receiver: automatic channel discovery, four strands, rave/electronic effects, VU tuning tools
// Starts with your known-working setup by default:
//   Strand 1: GPIO33, 10 LEDs, NEO_RGB + NEO_KHZ800
//   Strand 2: GPIO25
//   Strand 3: GPIO26
//   Strand 4: GPIO27
// Then accepts pin/count/type updates from the sender webpage Hardware tab.

#include <Arduino.h>
#include <esp_system.h>
#include "LuminaProtocol.h"
#include "lighting/Color.h"
#include "lighting/EffectState.h"
#include "lighting/LedOutput.h"
#include "lighting/Renderer.h"
#include "network/EspNowReceiver.h"

// v3 layout: setup() and the frame loop live here; each subsystem has its own module.

static constexpr uint32_t FRAME_INTERVAL_MS = 16;   // up to ~60 FPS; LED show time may lower this
static constexpr uint32_t PACKET_TIMEOUT_MS = 1500;

// Frame and packet-loss statistics printed once per second.
uint32_t renderedFrames = 0;
uint32_t renderFps = 0;
uint32_t lastFpsMs = 0;
uint32_t renderUs = 0;
uint32_t renderUsMax = 0;
uint32_t showUs = 0;
uint32_t showUsMax = 0;

uint16_t lastSequence = 0;
uint32_t lostPackets = 0;
uint8_t lastBeatPulse = 0;
uint8_t beatFlash = 0;

static void setup_leds() {
  for (uint8_t s = 0; s < MAX_STRANDS; s++) {
    rt[s] = {};
    configure_strand(s, DEFAULT_LED_PINS[s], DEFAULT_LED_COUNTS[s], DEFAULT_TYPE[s]);
  }
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
