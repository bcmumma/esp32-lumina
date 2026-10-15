// Lumina v22 home-Wi-Fi sender: rave/electronic effects, VU tools in Audio Tune, beat detector disabled, no recorder
// v3 layout: setup() and loop() only; each subsystem lives in its own module.
#include <Arduino.h>
#include <WiFi.h>
#include "LuminaProtocol.h"
#include "config/ConfigManager.h"
#include "audio/PCM1808.h"
#include "audio/AudioAnalyzer.h"
#include "network/WiFiManager.h"
#include "network/EspNowSender.h"
#include "web/LuminaWebServer.h"
#include "network/ArtNetReceiver.h"
#include "lighting/Lumina16Control.h"

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Lumina v3 sender: home-Wi-Fi PCM1808 sender, dual-slot CRC config (PlatformIO build of v2/v22)");
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
  setup_wifi();
  setup_espnow();
  setup_web_server();
  setup_artnet();
  setup_i2s();

  Serial.print("Ready. Open http://");
  Serial.println(WiFi.localIP());
  Serial.println("Recorder removed. Commands use immediate delta updates; /api/live is lightweight diagnostics.");
}

void loop() {
  uint32_t loopStartedUs = micros();

  maintain_wifi_connection();
  artnet_service(millis());

  // Service short browser commands before entering the blocking I2S read.
  server.handleClient();

  size_t got = I2S.readBytes((char *)raw_samples, sizeof(raw_samples));
  int n32 = got / sizeof(int32_t);
  int frames = n32 / 2;

  if (frames > 0) {
    uint32_t analyzeStartedUs = micros();
    AudioLedPacket packet = analyze_audio_block(frames);
    control_apply(packet, millis());  // strobe safety bits; LUMINA-16 when Art-Net control is on
    lastAnalyzeUs = micros() - analyzeStartedUs;
    if (lastAnalyzeUs > maxAnalyzeUs) maxAnalyzeUs = lastAnalyzeUs;

    maybe_send_espnow(packet);
    last_packet = packet;
  }

  // Service the web server again immediately after audio/FFT work.
  server.handleClient();

  uint32_t now = millis();
  update_packet_rate(now);
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

