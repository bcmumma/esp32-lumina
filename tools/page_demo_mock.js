// Simulated Lumina API for the controller-page demo (tools/build_page.py --demo).
// Replaces fetch() for /api/* with in-memory state and fake live audio (kicks at 128 BPM).
// Never included in the firmware build.
(function () {
  const pins = [33, 25, 26, 27];
  const state = {
    led: {
      activeStrands: 4, maxLedsPerStrand: 300, globalBrightness: 255, globalHue: 0, colorMap: 0,
      redScale: 128, greenScale: 255, blueScale: 126, gamma10: 20,
      strand: pins.map((pin) => ({ enabled: 1, pin, ledType: 0, ledCount: 150, mode: 33, brightness: 255, hue: 0, saturation: 255,
        speed: 0, density: 0, sensitivity: 160, strobe: 170, twinkle: 0, direction: 0 })),
    },
    cfg: { shiftTo16: 18, volumeGain: 0.55, bassGain: 0.9, midGain: 0.7, trebleGain: 1.85, lrGain: 1.15, volumeFloor: 317, bassFloor: 228,
      midFloor: 189, trebleFloor: 192, attack: 0.6, release: 0.08, peakDecay: 0.995, beatVolMin: 171, beatBassMin: 80, beatSensitivity: 1.75,
      beatMinOnset: 0.098, beatTempoAssist: 1, beatPredictStrength: 0.83, beatPredictToleranceMs: 85, beatCooldownMs: 210, beatHoldMs: 210,
      beatMinBpm: 110, beatMaxBpm: 180, packetIntervalMs: 34 },
  };
  const ctl = { source: "auto", active: "audio", reason: "auto: no fresh art-net, fallback", autoFallback: "audio", timeoutMs: 1000,
    standaloneLevel: 128, switches: 7, artnetAgeMs: 84641, applyingArtnet: false, appliedPackets: 46847, address: 1,
    brightnessCap: 255, brightnessCapMin: 8, brightnessCapMax: 255, strobeEnabled: false, strobeHz: 3, strobeMaxHz: 15, kickFlash: 0, fixture: 153, fixtureName: "10x3 RGB", fixtureCells: 30, fixtureChannels: 90, cellFrames: 0, cellPacketsSent: 0, cellFlashes: 0, hybridHoldMs: 10000, fxEnabled: 1, fxHit: 1, beamStrength: 255, beamSpread: 3, fxAddress: 97,
    fxLive: { beam: 230, pos: 140, moving: 1, pulsing: 1, sparkle: 0, edges: 0 } };
  const L = (id, name, mode, fixtures, cells, dimmer, strobe, mirror, hard) => {
    const per = (dimmer ? 1 : 0) + 3 * cells + (strobe ? 1 : 0);
    return { id, name, mode, fixtures, cells, perFixture: per, total: per * fixtures, stripCells: fixtures * cells * (mirror ? 2 : 1),
      dimmer: +dimmer, strobe: +strobe, mirror: +mirror, hard: +hard };
  };
  const FIXTURES = [L(1, "30x1 RGB", "1 Cell", 30, 1), L(2, "15x2 RGB", "2 Cell", 15, 2), L(3, "10x3 RGB", "3 Cell", 10, 3),
    L(4, "6x5 RGB", "5 Cell", 6, 5), L(5, "3x10 RGB", "10 Cell", 3, 10), L(6, "1x30 RGB", "30 Cell", 1, 30),
    L(7, "10x3 RGB Blocks", "3 Cell", 10, 3, 0, 0, 0, 1), L(8, "6x5 RGB Blocks", "5 Cell", 6, 5, 0, 0, 0, 1),
    L(9, "30x1 Dim", "1 Cell Dim", 30, 1, 1), L(10, "10x3 Dim", "3 Cell Dim", 10, 3, 1), L(11, "6x5 Dim", "5 Cell Dim", 6, 5, 1),
    L(12, "30x1 Dim Strobe", "1 Cell Dim Strobe", 30, 1, 1, 1), L(13, "10x3 Dim Strobe", "3 Cell Dim Strobe", 10, 3, 1, 1),
    L(14, "6x5 Dim Strobe", "5 Cell Dim Strobe", 6, 5, 1, 1), L(15, "Mirror 15x1 RGB", "1 Cell", 15, 1, 0, 0, 1),
    L(16, "Mirror 5x3 RGB", "3 Cell", 5, 3, 0, 0, 1), L(17, "Mirror 5x3 Dim Strobe", "3 Cell Dim Strobe", 5, 3, 1, 1, 1),
    L(18, "Fine 60x1 RGB", "1 Cell", 60, 1), L(19, "Fine 30x2 RGB", "2 Cell", 30, 2), L(20, "Fine 15x4 RGB", "4 Cell", 15, 4)];
  const t0 = performance.now();
  let pulse = 0, lastKick = -1;

  function live() {
    const t = (performance.now() - t0) / 1000;
    const beat = 60 / 128, k = Math.floor(t / beat), ph = (t % beat) / beat;
    if (k !== lastKick) { lastKick = k; pulse = (pulse + 1) & 255; }
    const standalone = ctl.active === "standalone";
    const lvl = (v) => Math.max(0, Math.min(255, Math.round(v)));
    const bass = standalone ? ctl.standaloneLevel : lvl(40 + 200 * Math.exp(-ph * 5));
    const mid = standalone ? ctl.standaloneLevel : lvl(110 + 50 * Math.sin(t * 2.1) + 20 * Math.random());
    const hat = ((t * 4) % 1) < 0.15 ? 170 : 60;
    const treble = standalone ? ctl.standaloneLevel : lvl(hat + 25 * Math.random());
    const vol = standalone ? ctl.standaloneLevel : lvl((bass + mid + treble) / 3 + 20);
    return { ms: Math.round(t * 1000), volume: vol, bass, mid, treble, left: lvl(vol + 12 * Math.sin(t * 0.7)), right: lvl(vol - 12 * Math.sin(t * 0.7)),
      beat: ph < 0.1 ? 255 : 0, beatPulse: standalone ? 0 : pulse, clip: 0, rawVolume: vol * 9.1, rawBass: bass * 6.3, rawMid: mid * 4.2, rawTreble: treble * 3.9,
      pps: 29, enqOk: 41210 + k, enqErr: 0, busySkip: 3, cbOk: 41190 + k, cbFail: 12, lastErr: 0, wifiConnected: 1, wifiRssi: -71,
      wifiIp: "192.0.2.10", wifiChannel: 6, freeHeap: 216400, minHeap: 201000, fftUs: 0, fftAvgUs: 0, analyzeUs: 205, httpUs: 900, loopUs: 5400 };
  }
  function applyControl(q) {
    if (q.has("source")) ctl.source = q.get("source");
    if (q.has("fallback")) ctl.autoFallback = q.get("fallback");
    if (q.has("standalone")) ctl.standaloneLevel = +q.get("standalone");
    if (q.has("strobeHz")) ctl.strobeHz = Math.max(1, Math.min(15, +q.get("strobeHz")));
    if (q.has("cells")) { ctl.fixture = Math.max(0, Math.min(150, +q.get("cells"))); ctl.fixtureCells = ctl.fixture; }
    if (q.has("fixture")) {
      const f = q.get("fixture"), p = FIXTURES.find((x) => x.id === +f);
      if (f === "lumina16") { ctl.fixture = 0; ctl.fixtureCells = 0; }
      else if (p) { ctl.fixture = 150 + p.id; ctl.fixtureCells = p.stripCells; }
    }
    if (q.has("fx")) ctl.fxEnabled = +q.get("fx");
    if (q.has("fxHit")) ctl.fxHit = +q.get("fxHit");
    if (q.has("beam")) ctl.beamStrength = +q.get("beam");
    if (q.has("spread")) ctl.beamSpread = +q.get("spread");
    if (q.has("kickFlash")) ctl.kickFlash = Math.max(0, Math.min(255, +q.get("kickFlash")));
    if (q.has("strobe")) ctl.strobeEnabled = q.get("strobe") !== "0";
    if (q.has("cap")) ctl.brightnessCap = Math.max(8, Math.min(255, +q.get("cap")));
    ctl.active = ctl.source === "auto" ? ctl.autoFallback : ctl.source;
    ctl.reason = ctl.source === "auto" ? "auto: no fresh art-net, fallback" : ctl.source === "artnet" ? "artnet: no frame yet" : "selected";
    ctl.artnetAgeMs = 84641 + Math.round(performance.now() - t0);
    return ctl;
  }
  function applyLed(q) {
    for (const [k, v] of q) {
      const m = /^s(\d)_(\w+)$/.exec(k);
      if (m) state.led.strand[+m[1]][m[2]] = +v; else if (k in state.led) state.led[k] = +v;
    }
    return { ok: true };
  }
  const realFetch = window.fetch.bind(window);
  window.fetch = async function (input, init) {
    const url = new URL(typeof input === "string" ? input : input.url, location.href);
    if (!url.pathname.startsWith("/api/")) return realFetch(input, init);
    await new Promise((r) => setTimeout(r, 30));
    const q = url.searchParams;
    let body;
    switch (url.pathname) {
      case "/api/state": body = Object.assign(live(), JSON.parse(JSON.stringify(state))); break;
      case "/api/live": body = live(); break;
      case "/api/control": body = applyControl(q); break;
      case "/api/fixtures": body = FIXTURES; break;
      case "/api/setLed": body = applyLed(q); break;
      case "/api/setTuning": for (const [k, v] of q) state.cfg[k] = +v; body = { ok: true }; break;
      case "/api/save": body = { saved: true, activeStrands: state.led.activeStrands }; break;
      default: body = { ok: true };
    }
    return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
  };
  const banner = document.createElement("div");
  banner.textContent = "Demo: simulated data, not connected to Lumina. Kicks at 128 BPM.";
  banner.style.cssText = "background:#2b3040;color:#ecebf3;font:13px system-ui,sans-serif;text-align:center;padding:6px 16px;";
  document.body.prepend(banner);
})();
