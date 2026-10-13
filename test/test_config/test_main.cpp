// Config-blob integrity tests. In-memory only: never calls load/save, so the
// board's saved settings (NVS "lumina_cfg") are not touched.
#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "config/ConfigManager.h"

void setUp() {}
void tearDown() {}

static PersistedConfigBlob make_valid_blob() {
  PersistedConfigBlob blob;
  memset(&blob, 0, sizeof(blob));
  blob.magic = PERSIST_MAGIC;
  blob.version = PERSIST_VERSION;
  blob.size = sizeof(PersistedConfigBlob);
  blob.sequence = 42;
  blob.tuning.volumeGain = 0.55f;
  blob.leds.activeStrands = 4;
  blob.crc32 = persisted_blob_crc(blob);
  return blob;
}

static void test_crc32_standard_check_value() {
  const char *msg = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL, config_crc32((const uint8_t *)msg, strlen(msg)));
}

static void test_valid_blob_accepted() {
  PersistedConfigBlob blob = make_valid_blob();
  TEST_ASSERT_TRUE(persisted_blob_valid(blob));
}

static void test_corrupted_byte_rejected() {
  PersistedConfigBlob blob = make_valid_blob();
  reinterpret_cast<uint8_t *>(&blob.tuning)[3] ^= 0x01;
  TEST_ASSERT_FALSE(persisted_blob_valid(blob));
}

static void test_wrong_magic_rejected() {
  PersistedConfigBlob blob = make_valid_blob();
  blob.magic ^= 1;
  blob.crc32 = persisted_blob_crc(blob);
  TEST_ASSERT_FALSE(persisted_blob_valid(blob));
}

static void test_wrong_size_rejected() {
  PersistedConfigBlob blob = make_valid_blob();
  blob.size -= 1;
  blob.crc32 = persisted_blob_crc(blob);
  TEST_ASSERT_FALSE(persisted_blob_valid(blob));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_crc32_standard_check_value);
  RUN_TEST(test_valid_blob_accepted);
  RUN_TEST(test_corrupted_byte_rejected);
  RUN_TEST(test_wrong_magic_rejected);
  RUN_TEST(test_wrong_size_rejected);
  UNITY_END();
}

void loop() {}
