// Source-router decision tests (pure logic).
#include <Arduino.h>
#include <unity.h>
#include "SourceRouter.h"

using router::Config;
using router::Decision;
using router::Reason;
using router::Source;

void setUp() {}
void tearDown() {}

static Config cfg(Source sel, Source fb = Source::Audio) { return {sel, fb, router::kDefaultTimeoutMs, 128, 0, 0}; }
// Phase 15 chain: dark detection and the HYBRID stage on.
static Config chain(Source sel = Source::Auto) {
  return {sel, Source::Audio, router::kDefaultTimeoutMs, 128, router::kDefaultDarkMs, router::kDefaultHybridHoldMs};
}

static void test_auto_fresh_uses_artnet() {
  Decision d = router::decide(cfg(Source::Auto), true, 30, 30);
  TEST_ASSERT_EQUAL(Source::Artnet, d.active);
  TEST_ASSERT_EQUAL(Reason::AutoArtnetFresh, d.reason);
  TEST_ASSERT_TRUE(d.applyArtnet);
  TEST_ASSERT_FALSE(d.standaloneLevels);
}

static void test_auto_tolerates_measured_stalls_then_falls_back() {
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(cfg(Source::Auto), true, 480, 480).active);   // Phase 11 worst stall
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(cfg(Source::Auto), true, 1000, 1000).active);  // boundary
  Decision d = router::decide(cfg(Source::Auto), true, 1001, 1001);
  TEST_ASSERT_EQUAL(Source::Audio, d.active);
  TEST_ASSERT_EQUAL(Reason::AutoFallback, d.reason);
  TEST_ASSERT_FALSE(d.applyArtnet);
}

static void test_auto_never_had_artnet_falls_back() {
  Decision d = router::decide(cfg(Source::Auto), false, 0, router::kNeverLit);
  TEST_ASSERT_EQUAL(Source::Audio, d.active);
  TEST_ASSERT_EQUAL(Reason::AutoFallback, d.reason);
}

static void test_auto_fallback_standalone() {
  Decision d = router::decide(cfg(Source::Auto, Source::Standalone), true, 5000, 5000);
  TEST_ASSERT_EQUAL(Source::Standalone, d.active);
  TEST_ASSERT_TRUE(d.standaloneLevels);
  // Fallback can't be ARTNET or AUTO: sanitized to AUDIO.
  TEST_ASSERT_EQUAL(Source::Audio, router::decide(cfg(Source::Auto, Source::Artnet), false, 0, router::kNeverLit).active);
  TEST_ASSERT_EQUAL(Source::Audio, router::sanitize_fallback(Source::Auto));
}

static void test_auto_returns_when_frames_resume() {
  TEST_ASSERT_EQUAL(Source::Audio, router::decide(cfg(Source::Auto), true, 4000, 4000).active);
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(cfg(Source::Auto), true, 15, 15).active);
}

static void test_forced_artnet_holds_last_frame() {
  Decision fresh = router::decide(cfg(Source::Artnet), true, 20, 20);
  TEST_ASSERT_EQUAL(Reason::Selected, fresh.reason);
  TEST_ASSERT_TRUE(fresh.applyArtnet);
  Decision stale = router::decide(cfg(Source::Artnet), true, 60000, 60000);
  TEST_ASSERT_EQUAL(Source::Artnet, stale.active);
  TEST_ASSERT_EQUAL(Reason::ArtnetStaleHold, stale.reason);
  TEST_ASSERT_TRUE(stale.applyArtnet);  // hold the last frame, don't fall back
  Decision none = router::decide(cfg(Source::Artnet), false, 0, router::kNeverLit);
  TEST_ASSERT_EQUAL(Reason::ArtnetNoData, none.reason);
  TEST_ASSERT_FALSE(none.applyArtnet);
}

static void test_forced_audio_and_standalone_ignore_artnet() {
  Decision a = router::decide(cfg(Source::Audio), true, 10, 10);
  TEST_ASSERT_EQUAL(Source::Audio, a.active);
  TEST_ASSERT_FALSE(a.applyArtnet);
  Decision s = router::decide(cfg(Source::Standalone), true, 10, 10);
  TEST_ASSERT_EQUAL(Source::Standalone, s.active);
  TEST_ASSERT_FALSE(s.applyArtnet);
  TEST_ASSERT_TRUE(s.standaloneLevels);
}

static void test_apply_standalone_levels() {
  AudioLedPacket p = {};
  p.volume = 3; p.bass = 200; p.beat = 255; p.clip = 9; p.beatPulse = 7;
  router::apply_standalone(p, 128);
  TEST_ASSERT_EQUAL_UINT8(128, p.volume);
  TEST_ASSERT_EQUAL_UINT8(128, p.bass);
  TEST_ASSERT_EQUAL_UINT8(128, p.mid);
  TEST_ASSERT_EQUAL_UINT8(128, p.treble);
  TEST_ASSERT_EQUAL_UINT8(128, p.left);
  TEST_ASSERT_EQUAL_UINT8(128, p.right);
  TEST_ASSERT_EQUAL_UINT8(0, p.beat);
  TEST_ASSERT_EQUAL_UINT8(0, p.clip);
  TEST_ASSERT_EQUAL_UINT8(7, p.beatPulse);  // untouched
}

static void test_parse_and_names() {
  Source s;
  TEST_ASSERT_TRUE(router::parse("AUTO", s)); TEST_ASSERT_EQUAL(Source::Auto, s);
  TEST_ASSERT_TRUE(router::parse("artnet", s)); TEST_ASSERT_EQUAL(Source::Artnet, s);
  TEST_ASSERT_TRUE(router::parse("Audio", s)); TEST_ASSERT_EQUAL(Source::Audio, s);
  TEST_ASSERT_TRUE(router::parse("standalone", s)); TEST_ASSERT_EQUAL(Source::Standalone, s);
  TEST_ASSERT_FALSE(router::parse("dmx", s));
  TEST_ASSERT_FALSE(router::parse(nullptr, s));
  TEST_ASSERT_EQUAL_STRING("artnet", router::name(Source::Artnet));
  TEST_ASSERT_EQUAL_STRING("auto", router::name(Source::Auto));
}

static void test_chain_lit_frames_use_soundswitch() {
  Decision d = router::decide(chain(), true, 20, 20);
  TEST_ASSERT_EQUAL(Source::Artnet, d.active);
  TEST_ASSERT_TRUE(d.applyArtnet);
  TEST_ASSERT_FALSE(d.hybridColour);
}

static void test_chain_stop_goes_hybrid_then_audio() {
  // SoundSwitch quit: frames go stale. Within the 1 s timeout it holds, then HYBRID until 10 s, then AUDIO.
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(chain(), true, 900, 900).active);
  Decision h = router::decide(chain(), true, 1100, 1100);
  TEST_ASSERT_EQUAL(Source::Hybrid, h.active);
  TEST_ASSERT_EQUAL(Reason::AutoHybrid, h.reason);
  TEST_ASSERT_TRUE(h.hybridColour);
  TEST_ASSERT_FALSE(h.applyArtnet);
  TEST_ASSERT_EQUAL(Source::Hybrid, router::decide(chain(), true, 10000, 10000).active);
  Decision a = router::decide(chain(), true, 10001, 10001);
  TEST_ASSERT_EQUAL(Source::Audio, a.active);
  TEST_ASSERT_EQUAL(Reason::AutoFallback, a.reason);
}

static void test_chain_dark_frames_go_hybrid() {
  // Fresh frames but all dark (SoundSwitch blackout quirk): SoundSwitch for 3 s, then HYBRID, then AUDIO.
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(chain(), true, 30, 2900).active);
  TEST_ASSERT_EQUAL(Source::Hybrid, router::decide(chain(), true, 30, 3100).active);
  TEST_ASSERT_EQUAL(Source::Audio, router::decide(chain(), true, 30, 12000).active);
  TEST_ASSERT_EQUAL(Source::Artnet, router::decide(chain(), true, 30, 0).active);  // lit again: back at once
}

static void test_chain_never_lit_uses_fallback() {
  TEST_ASSERT_EQUAL(Source::Audio, router::decide(chain(), false, 0, router::kNeverLit).active);
  TEST_ASSERT_EQUAL(Source::Audio, router::decide(chain(), true, 20, router::kNeverLit).active);  // dark since boot
}

static void test_selected_hybrid() {
  Decision d = router::decide(chain(Source::Hybrid), false, 0, router::kNeverLit);
  TEST_ASSERT_EQUAL(Source::Hybrid, d.active);
  TEST_ASSERT_EQUAL(Reason::Selected, d.reason);
  TEST_ASSERT_TRUE(d.hybridColour);
  Source s;
  TEST_ASSERT_TRUE(router::parse("Hybrid", s));
  TEST_ASSERT_EQUAL(Source::Hybrid, s);
  TEST_ASSERT_EQUAL_STRING("hybrid", router::name(Source::Hybrid));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_auto_fresh_uses_artnet);
  RUN_TEST(test_auto_tolerates_measured_stalls_then_falls_back);
  RUN_TEST(test_auto_never_had_artnet_falls_back);
  RUN_TEST(test_auto_fallback_standalone);
  RUN_TEST(test_auto_returns_when_frames_resume);
  RUN_TEST(test_forced_artnet_holds_last_frame);
  RUN_TEST(test_forced_audio_and_standalone_ignore_artnet);
  RUN_TEST(test_apply_standalone_levels);
  RUN_TEST(test_parse_and_names);
  RUN_TEST(test_chain_lit_frames_use_soundswitch);
  RUN_TEST(test_chain_stop_goes_hybrid_then_audio);
  RUN_TEST(test_chain_dark_frames_go_hybrid);
  RUN_TEST(test_chain_never_lit_uses_fallback);
  RUN_TEST(test_selected_hybrid);
  UNITY_END();
}

void loop() {}
