#include "ConfigManager.h"
#include "../util/Clamp.h"

Preferences prefs;
TuningConfig cfg;
static uint32_t persistedConfigSequence = 0;
bool configMigrationNeeded = false;

void set_default_config() {
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

uint32_t config_crc32(const uint8_t *data, size_t len) {
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

uint32_t persisted_blob_crc(const PersistedConfigBlob &blob) {
  return config_crc32(
    reinterpret_cast<const uint8_t *>(&blob),
    offsetof(PersistedConfigBlob, crc32)
  );
}

bool persisted_blob_valid(const PersistedConfigBlob &blob) {
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
    ledCfg.strand[s].mode = (uint8_t)clamp_int(ledCfg.strand[s].mode, 0, LED_MODE_MAX);
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

void load_config() {
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

bool save_config() {
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
