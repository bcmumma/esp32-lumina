// Tuning settings plus the dual-slot, CRC-protected config store (v20.1 design).
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../lighting/LightingState.h"

static constexpr uint8_t CONFIG_SCHEMA_VERSION = 19;

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

extern TuningConfig cfg;
extern Preferences prefs;
extern bool configMigrationNeeded;

void set_default_config();

// Pure helpers (no storage access); exposed for the on-device unit tests.
uint32_t config_crc32(const uint8_t *data, size_t len);
uint32_t persisted_blob_crc(const PersistedConfigBlob &blob);
bool persisted_blob_valid(const PersistedConfigBlob &blob);

void load_config();
bool save_config();
