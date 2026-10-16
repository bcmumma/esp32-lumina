#include "AudioAnalyzer.h"
#include <math.h>
#include "../config/ConfigManager.h"
#include "../lighting/LightingState.h"
#include "../util/Clamp.h"

#include "../network/EspNowSender.h"  // sequence_num

int16_t beat_mono_samples[BLOCK_FRAMES];

uint8_t beat_pulse_counter = 0;
hits::State hitState = {0.0f, 0.0f, true, false, 0, 0, 0};
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

// v22 effects react to band energy directly; beat detection is intentionally disabled.

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

// FFT scheduling/performance diagnostics.
uint8_t fftBlockCounter = 0;
uint32_t fftRunCount = 0;
uint32_t lastFftUs = 0;
uint32_t maxFftUs = 0;
uint32_t avgFftUs = 0;
uint32_t lastAnalyzeUs = 0;
uint32_t maxAnalyzeUs = 0;

static void update_beat_detector(AudioLedPacket &p, uint32_t now, bool runFft);

void reset_agc() {
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

void reset_beat_state() {
  hits::reset(hitState);
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

void init_fft_tables() {
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

AudioLedPacket analyze_audio_block(int frames) {
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
    // Phase 14: bass hits (transients, not tempo) drive beatPulse so the hit-driven modes react.
    hits::Config hc = hits::kDefaultConfig;
    hc.floor = cfg.bassFloor;
    if (hits::update(hitState, hc, raw_bass, p.ms)) beat_pulse_counter++;
    p.beat = (hitState.lastHitMs != 0 && p.ms - hitState.lastHitMs < HIT_HOLD_MS) ? 255 : 0;
    p.beatPulse = beat_pulse_counter;
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
  p.kickFlash = 0;  // set by control_apply()

  return p;
}
