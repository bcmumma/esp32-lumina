// Band levels, AGC, and the (disabled) FFT beat detector. Produces one packet per audio block.
#pragma once

#include <Arduino.h>
#include "LuminaProtocol.h"
#include "PCM1808.h"
#include "HitDetector.h"

static constexpr float PI_F = 3.14159265359f;
static constexpr int FFT_SIZE = 1024;
static constexpr int FFT_UPDATE_EVERY_BLOCKS = 2;
static constexpr int FFT_HOP = BLOCK_FRAMES * FFT_UPDATE_EVERY_BLOCKS;
static constexpr int FFT_BIN_COUNT = (FFT_SIZE / 2) + 1;
static constexpr int FFT_BAND_COUNT = 36;
static constexpr float FFT_MIN_HZ = 43.0f;
static constexpr float FFT_MAX_HZ = 12000.0f;
static constexpr bool ENABLE_BEAT_DETECTOR = false;
static constexpr uint32_t HIT_HOLD_MS = 80;  // packet `beat` stays 255 this long after a bass hit

extern int16_t beat_mono_samples[BLOCK_FRAMES];
extern uint8_t beat_pulse_counter;
extern hits::State hitState;  // bass-hit detector (Phase 14); hitState.count = hits since boot
extern AudioLedPacket last_packet;
extern float peak_volume;
extern float peak_bass;
extern float peak_mid;
extern float peak_treble;
extern float peak_left;
extern float peak_right;
extern float smooth_volume;
extern float smooth_bass;
extern float smooth_mid;
extern float smooth_treble;
extern float smooth_left;
extern float smooth_right;
extern float raw_volume;
extern float raw_bass;
extern float raw_mid;
extern float raw_treble;
extern float raw_left;
extern float raw_right;
extern float lp40;
extern float lp160;
extern float lp250;
extern float lp4000;
extern float fftWindow[FFT_SIZE];
extern float fftReal[FFT_SIZE];
extern float fftImag[FFT_SIZE];
extern int16_t fftMonoWindow[FFT_SIZE];
extern uint16_t fftFilledSamples;
extern bool fftReady;
extern bool fftTablesReady;
extern uint16_t fftBandStart[FFT_BAND_COUNT];
extern uint16_t fftBandEnd[FFT_BAND_COUNT];
extern float fftBandCenterHz[FFT_BAND_COUNT];
extern float fftBandLog[FFT_BAND_COUNT];
extern float prevBandLog[FFT_BAND_COUNT];
extern bool fftBandPrimed;
extern float prevLogVolume;
extern bool beatStatePrimed;
extern uint16_t beatWarmupBlocks;
extern float onsetMean;
extern float onsetDev;
extern float onsetPrev2;
extern float onsetPrev1;
extern float thresholdPrev2;
extern float thresholdPrev1;
extern uint32_t onsetMsPrev2;
extern uint32_t onsetMsPrev1;
extern uint32_t lastBeatMs;
extern uint32_t beatHoldUntilMs;
extern float beatPeriodMs;
extern float debugOnset;
extern float debugThreshold;
extern float debugScore;
extern float debugBpm;
extern float debugBeatAgeMs;
extern float debugBeatPeriodMs;
extern uint8_t debugCandidate;
extern uint8_t debugTempoAssist;
extern float debugSpectralFlux;
extern float debugLowFlux;
extern float debugHighFlux;
extern float debugFftPeakHz;
extern uint8_t debugFftReady;
extern uint8_t fftBlockCounter;
extern uint32_t fftRunCount;
extern uint32_t lastFftUs;
extern uint32_t maxFftUs;
extern uint32_t avgFftUs;
extern uint32_t lastAnalyzeUs;
extern uint32_t maxAnalyzeUs;

void init_fft_tables();
void reset_agc();
void reset_beat_state();
AudioLedPacket analyze_audio_block(int frames);
