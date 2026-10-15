#include "IndexHtml.h"
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>Lumina LED Controller V22</title>
  <style>
    :root {
      --bg:#0d1016;
      --card:#191e27;
      --card2:#202634;
      --line:#313949;
      --text:#f5f7fb;
      --muted:#aab3c2;
      --blue:#2d74ff;
      --green:#00d084;
      --yellow:#ffde59;
      --red:#ff4d4d;
    }
    * { box-sizing:border-box; }
    body {
      font-family: Arial, Helvetica, sans-serif;
      background:var(--bg);
      color:var(--text);
      margin:0;
      padding:12px;
      line-height:1.35;
    }
    .page { max-width:1180px; margin:0 auto; }
    h1 { margin:4px 0 4px 0; font-size:26px; }
    h2 { margin:0 0 10px 0; font-size:20px; }
    h3 { margin:0 0 8px 0; font-size:18px; }
    .small { color:var(--muted); font-size:13px; }
    .warn { color:var(--yellow); }
    .tabs {
      position:sticky;
      top:0;
      z-index:5;
      display:flex;
      gap:8px;
      overflow-x:auto;
      overscroll-behavior-x:contain;
      scrollbar-width:thin;
      background:rgba(13,16,22,0.97);
      padding:10px 0;
      backdrop-filter:blur(6px);
      -webkit-overflow-scrolling:touch;
    }
    .tabbtn {
      flex:0 0 auto;
      min-width:108px;
      background:#252b36;
      border:1px solid #3b4352;
      color:#ddd;
      border-radius:12px;
      padding:13px 10px;
      font-size:15px;
      font-weight:700;
      min-height:48px;
      white-space:nowrap;
    }
    .tabbtn.active { background:var(--blue); color:white; border-color:var(--blue); }
    .tabbtn.inactive-strand { opacity:0.58; }
    .tab { display:none; }
    .tab.active { display:block; }
    .card, .strand {
      background:var(--card);
      border:1px solid var(--line);
      border-radius:16px;
      padding:14px;
      margin-bottom:14px;
      box-shadow:0 2px 14px rgba(0,0,0,0.18);
    }
    .hero {
      background:linear-gradient(135deg,#1e2635,#131722);
      border-color:#3d4b63;
    }
    .grid {
      display:grid;
      grid-template-columns:165px minmax(0,1fr) 78px;
      gap:8px;
      align-items:center;
    }
    .grid2 { display:grid; grid-template-columns:repeat(auto-fit,minmax(310px,1fr)); gap:14px; }
    input[type=range] { width:100%; accent-color:var(--blue); }
    input, select {
      width:100%;
      background:#0b0d12;
      color:var(--text);
      border:1px solid #3b4352;
      border-radius:9px;
      padding:9px;
      font-size:16px;
    }
    button {
      background:var(--blue);
      color:white;
      border:0;
      border-radius:11px;
      padding:12px 14px;
      margin:4px 4px 4px 0;
      font-size:15px;
      font-weight:700;
      min-height:44px;
      touch-action:manipulation;
    }
    button.secondary { background:#3b4352; }
    button.danger { background:#a33; }
    .meterrow { display:grid; grid-template-columns:75px 1fr 50px; gap:8px; align-items:center; margin:7px 0; }
    .meter { height:22px; background:#07080a; border-radius:999px; overflow:hidden; border:1px solid #313742; }
    .bar { height:100%; width:0%; background:linear-gradient(90deg,#2d74ff,#00e676,#ffd54f,#ff5252); transition:width 70ms linear; }
    .beat { display:inline-block; width:24px; height:24px; border-radius:50%; background:#313744; vertical-align:middle; margin-left:8px; border:1px solid #566070; }
    .beat.on { background:#ffeb3b; box-shadow:0 0 18px #ffeb3b; border-color:#fff08a; }
    .beat.candidate.on { background:#00e5ff; box-shadow:0 0 18px #00e5ff; border-color:#8df6ff; }
    .beatline { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin:8px 0; }
    .beatpill { background:#0b0d12; border:1px solid #3b4352; border-radius:999px; padding:8px 12px; }
    pre { background:#080a0f; color:#d9f7ff; padding:12px; border-radius:10px; overflow-x:auto; white-space:pre-wrap; }
    .stat { display:inline-block; margin-right:14px; margin-bottom:5px; }
    .pill { display:inline-block; background:#0b0d12; border:1px solid #3b4352; border-radius:999px; padding:6px 10px; margin:2px; }
    .choice-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(118px,1fr)); gap:8px; margin:8px 0 12px 0; }
    .choice-btn {
      width:100%;
      min-height:58px;
      background:#242b38;
      color:#e8edf7;
      border:1px solid #3b4352;
      border-radius:14px;
      padding:10px 8px;
      margin:0;
      text-align:center;
      font-size:14px;
    }
    .choice-btn.active { background:var(--blue); border-color:#8ab2ff; color:white; box-shadow:0 0 0 2px rgba(45,116,255,0.22); }
    .color-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(76px,1fr)); gap:8px; margin:8px 0 12px 0; }
    .color-btn {
      min-height:54px;
      border-radius:14px;
      border:2px solid #3b4352;
      color:white;
      text-shadow:0 1px 4px rgba(0,0,0,0.9);
      font-weight:800;
      padding:8px 4px;
      margin:0;
    }
    .color-btn.active { border-color:white; box-shadow:0 0 0 3px rgba(255,255,255,0.18); }
    .section-title { margin-top:12px; margin-bottom:4px; color:#dce7ff; font-weight:800; }
    .mode-group-title { margin:14px 0 7px; color:#a9c4ff; font-size:13px; font-weight:900; letter-spacing:.08em; text-transform:uppercase; }
    .mode-group-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(118px,1fr)); gap:8px; margin:0 0 12px; }
    .mode-note { background:#101722; border-left:3px solid #2d74ff; border-radius:8px; padding:9px 10px; margin:8px 0 12px; color:#cbd8ef; font-size:13px; }
    .compact-controls { margin-top:8px; }
    .front-actions { display:flex; flex-wrap:wrap; gap:8px; }
    .front-actions button { flex:1 1 160px; }
    .toggle-row { display:grid; grid-template-columns:1fr 110px; gap:8px; align-items:center; }
    .hidden-input { display:none; }
    .status-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(145px,1fr)); gap:8px; }
    .status-box { background:#0b0d12; border:1px solid #3b4352; border-radius:10px; padding:9px; }
    .status-box b { display:block; font-size:19px; margin-top:2px; }
    .strand-heading { display:flex; align-items:center; justify-content:space-between; gap:10px; flex-wrap:wrap; }
    .strand-heading h2 { margin:0; }
    .toast {
      position:fixed;
      left:50%;
      bottom:18px;
      transform:translate(-50%,20px);
      z-index:30;
      max-width:calc(100vw - 24px);
      background:#111722;
      color:#fff;
      border:1px solid #42506a;
      border-radius:12px;
      padding:11px 16px;
      box-shadow:0 8px 28px rgba(0,0,0,.45);
      opacity:0;
      pointer-events:none;
      transition:opacity .18s ease, transform .18s ease;
      text-align:center;
      font-weight:700;
    }
    .toast.show { opacity:1; transform:translate(-50%,0); }
    .toast.error { background:#541f28; border-color:#c74d61; }
    .save-note { color:var(--green); font-weight:700; }
    @media (max-width:760px) {
      body { padding:10px; }
      h1 { font-size:22px; }
      .tabs { margin-left:-2px; margin-right:-2px; gap:7px; }
      .tabbtn { min-width:102px; min-height:52px; font-size:14px; padding:10px 9px; }
      .card, .strand { padding:12px; border-radius:14px; }
      .grid { grid-template-columns:1fr; gap:5px; }
      .grid > span { text-align:right; color:#dce7ff; margin-bottom:7px; }
      .grid2 { grid-template-columns:1fr; }
      .choice-grid { grid-template-columns:repeat(2,1fr); }
      .color-grid { grid-template-columns:repeat(3,1fr); }
      .meterrow { grid-template-columns:66px 1fr 44px; }
      button { width:100%; margin-right:0; }
      .front-actions { display:block; }
      .front-actions button { width:100%; }
    }
  </style>
</head>
<body>
<div class="page">
  <h1>Lumina LED Controller V22</h1>
  <div class="small">Home Wi-Fi mode. Your phone or laptop keeps internet access while controlling Lumina. For best response, keep the controller and ESP32 on the same SSID/VLAN.</div>

  <div class="tabs" aria-label="Lumina control pages">
    <button class="tabbtn active" id="btn_all" onclick="showTab('all')">All LEDs</button>
    <button class="tabbtn" id="btn_strand0" onclick="showTab('strand0')">Strand 1</button>
    <button class="tabbtn" id="btn_strand1" onclick="showTab('strand1')">Strand 2</button>
    <button class="tabbtn" id="btn_strand2" onclick="showTab('strand2')">Strand 3</button>
    <button class="tabbtn" id="btn_strand3" onclick="showTab('strand3')">Strand 4</button>
    <button class="tabbtn" id="btn_hardware" onclick="showTab('hardware')">Hardware</button>
    <button class="tabbtn" id="btn_tuning" onclick="showTab('tuning')">Audio Tune</button>
  </div>

  <div id="tab_all" class="tab active">
    <div class="card hero">
      <h2>Control All LEDs</h2>
      <p class="small">The simple phone-friendly page. Changes apply to every active strand. Use a Strand tab for individual looks, Hardware for setup/calibration, and Audio Tune for music response.</p>
      <div class="front-actions">
        <button onclick="sendLedNow(true)">Apply All LEDs</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>

    <div class="card">
      <h2>Control Source &amp; Strobe Safety</h2>
      <p class="small">Who drives the lights. <b>Auto</b> follows SoundSwitch (Art-Net) while it sends, and falls back after about 1 s without it. Source, fallback, strobe rate and standalone level are saved immediately.</p>
      <div class="front-actions">
        <button id="src_auto" onclick="setSource('auto')">Auto</button>
        <button id="src_artnet" onclick="setSource('artnet')">Art-Net</button>
        <button id="src_audio" onclick="setSource('audio')">Audio</button>
        <button id="src_standalone" onclick="setSource('standalone')">Standalone</button>
      </div>
      <p id="controlStatus" class="small">Active source: loading...</p>
      <div class="grid">
        <label>Auto fallback</label>
        <select id="ctlFallback" onchange="controlCall('fallback=' + this.value)"><option value="audio">Audio</option><option value="standalone">Standalone</option></select><span></span>
        <label>Standalone level</label><input id="ctlStandalone" type="range" min="0" max="255" step="1" onchange="controlCall('standalone=' + this.value)"><span id="ctlStandaloneVal"></span>
        <label>Strobe max rate</label>
        <select id="ctlStrobeHz" onchange="controlCall('strobeHz=' + this.value)"><option>1</option><option>2</option><option>3</option><option>4</option><option>5</option><option>6</option><option>7</option><option>8</option></select><span>flashes/s</span>
      </div>
      <div class="front-actions">
        <button id="ctlStrobeBtn" onclick="toggleStrobe()">Strobe: off</button>
      </div>
      <p class="small warn">Photosensitivity warning: flashing at about 3-30 flashes/s can trigger seizures. Strobe is off at every boot; keep it off near people who may be affected.</p>
    </div>

    <div class="card">
      <h2>Master Controls</h2>
      <div class="grid">
        <label>Global brightness</label><input id="globalBrightness" type="range" min="0" max="255" step="1"><span id="globalBrightnessVal"></span>
        <label>Global hue shift</label><input id="globalHue" type="range" min="0" max="255" step="1"><span id="globalHueVal"></span>
      </div>
      <div class="section-title">Quick global hue shift</div>
      <div class="color-grid" id="globalColorButtons"></div>
    </div>

    <div class="card">
      <h2>All Active Strands</h2>
      <p class="small">These controls copy one mode and color to every strand selected by Active Strands on the Hardware page.</p>
      <div class="section-title">Mode</div>
      <div id="allModeButtons"></div>
      <div class="section-title">Color</div>
      <div class="color-grid" id="allColorButtons"></div>
      <div class="compact-controls grid">
        <label>Brightness</label><input id="all_brightness" type="range" min="0" max="255" step="1"><span id="all_brightnessVal"></span>
        <label>Speed</label><input id="all_speed" type="range" min="0" max="255" step="1"><span id="all_speedVal"></span>
        <label>Density</label><input id="all_density" type="range" min="0" max="255" step="1"><span id="all_densityVal"></span>
        <label>Audio sensitivity</label><input id="all_sensitivity" type="range" min="0" max="255" step="1"><span id="all_sensitivityVal"></span>
        <label>Strobe/decay</label><input id="all_strobe" type="range" min="0" max="255" step="1"><span id="all_strobeVal"></span>
        <label>Twinkle/glitter</label><input id="all_twinkle" type="range" min="0" max="255" step="1"><span id="all_twinkleVal"></span>
        <label>Direction</label><input id="all_direction" type="range" min="0" max="2" step="1"><span id="all_directionVal"></span>
      </div>
      <div class="front-actions">
        <button onclick="copyStrandOneToAll()">Copy Strand 1 to All</button>
        <button onclick="sendLedNow(true)">Apply All</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>
  </div>

  <div id="tab_strand0" class="tab"><div id="strandControl_0"></div></div>
  <div id="tab_strand1" class="tab"><div id="strandControl_1"></div></div>
  <div id="tab_strand2" class="tab"><div id="strandControl_2"></div></div>
  <div id="tab_strand3" class="tab"><div id="strandControl_3"></div></div>

  <div id="tab_hardware" class="tab">
    <div class="card">
      <h2>LED Hardware Setup</h2>
      <div class="grid">
        <label>Active strands</label><input id="activeStrands" type="range" min="1" max="4" step="1"><span id="activeStrandsVal"></span>
        <label>Max LEDs/strand</label><input id="maxLedsPerStrand" type="number" min="1" max="1000" step="1"><span id="maxLedsPerStrandVal"></span>
      </div>
      <p class="small warn">Changing pins, LED count, or LED type causes the receiver to reconfigure. Save Configuration writes the complete current UI state to flash.</p>
      <div class="front-actions">
        <button onclick="sendLedNow()">Apply Hardware</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>
    <div class="card">
      <h2>Color Calibration</h2>
      <p class="small">Use this only when the color buttons do not match the real strip. The test buttons temporarily put every active strand in Solid Color mode so Red, Green, Blue, and White can be verified directly.</p>
      <input id="colorMap" type="hidden" value="0"><span id="colorMapVal" style="display:none"></span>
      <div class="section-title">Channel map</div>
      <div class="mode-grid color-map-grid" id="colorMapButtons"></div>
      <div class="grid">
        <label>Red scale</label><input id="redScale" type="range" min="0" max="255" step="1"><span id="redScaleVal"></span>
        <label>Green scale</label><input id="greenScale" type="range" min="0" max="255" step="1"><span id="greenScaleVal"></span>
        <label>Blue scale</label><input id="blueScale" type="range" min="0" max="255" step="1"><span id="blueScaleVal"></span>
        <label>Gamma x10</label><input id="gamma10" type="range" min="10" max="30" step="1"><span id="gamma10Val"></span>
      </div>
      <div class="front-actions">
        <button onclick="setCalibrationTest(0)">Test Red</button>
        <button onclick="setCalibrationTest(96)">Test Green</button>
        <button onclick="setCalibrationTest(165)">Test Blue</button>
        <button onclick="setCalibrationTest(0,0)">Test White</button>
        <button class="secondary" onclick="resetCalibration()">Reset Calibration</button>
        <button onclick="saveAll()">Save Calibration</button>
      </div>
    </div>

    <div class="card">
      <h2>ESP-NOW Status</h2>
      <div class="status-grid">
        <div class="status-box">Packets/sec <b id="pps">0</b></div>
        <div class="status-box">Queued OK <b id="enqOk">0</b></div>
        <div class="status-box">Queue errors <b id="enqErr">0</b></div>
        <div class="status-box">Busy skips <b id="busySkip">0</b></div>
        <div class="status-box">Callback OK <b id="cbOk">0</b></div>
        <div class="status-box">Callback fail <b id="cbFail">0</b></div>
        <div class="status-box">Last ESP err <b id="lastErr">0</b></div>
        <div class="status-box">Wi-Fi RSSI <b id="wifiRssi">0</b></div>
        <div class="status-box">Local IP <b id="wifiIp">0.0.0.0</b></div>
        <div class="status-box">Wi-Fi channel <b id="wifiChannel">0</b></div>
        <div class="status-box">Free heap <b id="freeHeap">0</b></div>
        <div class="status-box">FFT time us <b id="fftUs">0</b></div>
        <div class="status-box">Analyze time us <b id="analyzeUs">0</b></div>
        <div class="status-box">HTTP time us <b id="httpUs">0</b></div>
        <div class="status-box">Loop time us <b id="loopUs">0</b></div>
      </div>
      <p class="small">Use <code>/health</code> for Uptime Kuma. It is much lighter than polling <code>/api/state</code>.</p>
    </div>

    <div class="grid2" id="hardwareStrandControls"></div>
  </div>

  <div id="tab_tuning" class="tab">
    <div class="card hero">
      <h2>Input Audio Tuning</h2>
      <div id="meters_tuning"></div>
      <p class="small" id="rawText"></p>
    </div>

    <div class="card">
      <h2>Audio Calibration & VU Preview</h2>
      <p class="small">VU meters live here as tuning tools and are intentionally hidden from the normal lighting-mode list. Use the clean baseline, tune silence floors first, then compare overall, bass, mid, and treble response.</p>
      <div class="front-actions">
        <button onclick="applyCleanAudioBaseline()">Load Clean Audio Baseline</button>
        <button onclick="setVuPreview(6,'Overall VU')">Overall VU on Strand 1</button>
        <button onclick="setVuPreview(11,'Bass VU')">Bass VU on Strand 1</button>
        <button onclick="setVuPreview(12,'Mid VU')">Mid VU on Strand 1</button>
        <button onclick="setVuPreview(13,'Treble VU')">Treble VU on Strand 1</button>
        <button onclick="setAllOverallVu()">Overall VU on All Strands</button>
        <button onclick="setFourMeterLayout()">4-Strand VU Layout</button>
        <button class="secondary" onclick="restoreSolidColor()">Restore Solid Color</button>
        <button onclick="saveAll()">Save Configuration</button>
      </div>
    </div>

    <div class="card">
      <h2>Level tuning</h2>
      <div class="grid">
        <label>Shift to 16</label><input id="shiftTo16" type="range" min="13" max="18" step="1"><span id="shiftTo16Val"></span>
        <label>Volume gain</label><input id="volumeGain" type="range" min="0.1" max="8" step="0.05"><span id="volumeGainVal"></span>
        <label>Bass gain</label><input id="bassGain" type="range" min="0.1" max="12" step="0.05"><span id="bassGainVal"></span>
        <label>Mid gain</label><input id="midGain" type="range" min="0.1" max="12" step="0.05"><span id="midGainVal"></span>
        <label>Treble gain</label><input id="trebleGain" type="range" min="0.1" max="12" step="0.05"><span id="trebleGainVal"></span>
        <label>L/R gain</label><input id="lrGain" type="range" min="0.1" max="8" step="0.05"><span id="lrGainVal"></span>
        <label>Volume floor</label><input id="volumeFloor" type="range" min="0" max="500" step="1"><span id="volumeFloorVal"></span>
        <label>Bass floor</label><input id="bassFloor" type="range" min="0" max="500" step="1"><span id="bassFloorVal"></span>
        <label>Mid floor</label><input id="midFloor" type="range" min="0" max="500" step="1"><span id="midFloorVal"></span>
        <label>Treble floor</label><input id="trebleFloor" type="range" min="0" max="500" step="1"><span id="trebleFloorVal"></span>
      </div>
    </div>

    <div class="card">
      <h2>AGC / smoothing</h2>
      <div class="grid">
        <label>Attack</label><input id="attack" type="range" min="0.02" max="1" step="0.01"><span id="attackVal"></span>
        <label>Release</label><input id="release" type="range" min="0.01" max="0.8" step="0.01"><span id="releaseVal"></span>
        <label>Peak decay</label><input id="peakDecay" type="range" min="0.900" max="0.999" step="0.001"><span id="peakDecayVal"></span>
      </div>
    </div>

    <div class="card" style="display:none">
      <h2>Beat detection (disabled in v22)</h2>
      <div class="beatline">
        <span class="beatpill">Final beat <span id="beatLightFinal" class="beat"></span></span>
        <span class="beatpill">Onset candidate <span id="beatLightCandidate" class="beat candidate"></span></span>
      </div>
      <div class="grid">
        <label>Beat volume min</label><input id="beatVolMin" type="range" min="0" max="255" step="1"><span id="beatVolMinVal"></span>
        <label>Beat bass min</label><input id="beatBassMin" type="range" min="0" max="255" step="1"><span id="beatBassMinVal"></span>
        <label>Beat sensitivity</label><input id="beatSensitivity" type="range" min="0.30" max="3.00" step="0.01"><span id="beatSensitivityVal"></span>
        <label>Beat min onset</label><input id="beatMinOnset" type="range" min="0.000" max="0.300" step="0.001"><span id="beatMinOnsetVal"></span>
        <label>Tempo assist</label><input id="beatTempoAssist" type="range" min="0" max="1" step="1"><span id="beatTempoAssistVal"></span>
        <label>Predict strength</label><input id="beatPredictStrength" type="range" min="0.20" max="1.20" step="0.01"><span id="beatPredictStrengthVal"></span>
        <label>Predict tolerance ms</label><input id="beatPredictToleranceMs" type="range" min="20" max="220" step="5"><span id="beatPredictToleranceMsVal"></span>
        <label>Cooldown ms</label><input id="beatCooldownMs" type="range" min="60" max="500" step="5"><span id="beatCooldownMsVal"></span>
        <label>Hold ms</label><input id="beatHoldMs" type="range" min="20" max="300" step="5"><span id="beatHoldMsVal"></span>
        <label>Min BPM</label><input id="beatMinBpm" type="range" min="40" max="150" step="1"><span id="beatMinBpmVal"></span>
        <label>Max BPM</label><input id="beatMaxBpm" type="range" min="100" max="240" step="1"><span id="beatMaxBpmVal"></span>
        <label>Packet interval ms</label><input id="packetIntervalMs" type="range" min="20" max="120" step="1"><span id="packetIntervalMsVal"></span>
      </div>
      <p class="small" id="beatDebugText"></p>
    </div>

    <div class="card">
      <h2>Actions</h2>
      <button onclick="resetAgc()">Reset AGC</button>
      <button onclick="saveAll()">Save Configuration</button>
      <button class="secondary" onclick="copyAudioSettings()">Copy Audio Settings</button>
      <button class="danger" onclick="loadDefaults()">Defaults</button>
    </div>

    <div class="card">
      <h2>Audio values to write down</h2>
      <pre id="settingsText"></pre>
    </div>
  </div>

</div>
<div id="toast" class="toast" role="status" aria-live="polite"></div>

<script>
const meterNames = ["volume", "bass", "mid", "treble", "left", "right"];
const tuningIds = [
  "shiftTo16",
  "volumeGain", "bassGain", "midGain", "trebleGain", "lrGain",
  "volumeFloor", "bassFloor", "midFloor", "trebleFloor",
  "attack", "release", "peakDecay",
  "beatVolMin", "beatBassMin", "beatSensitivity", "beatMinOnset",
  "beatTempoAssist", "beatPredictStrength", "beatPredictToleranceMs",
  "beatCooldownMs", "beatHoldMs", "beatMinBpm", "beatMaxBpm",
  "packetIntervalMs"
];
const ledModes = [
  [9,  "Solid Color",       "general"],
  [15, "Aurora Drift",      "general"],
  [16, "Starfield",         "general"],
  [17, "Neon Scanner",      "general"],
  [0,  "Off",               "general"],

  [18, "Laser Sweep",       "rave"],
  [19, "Rave Chase",        "rave"],
  [20, "Bass Cannons",      "rave"],
  [21, "DnB Chopper",       "rave"],
  [22, "Acid Chase",        "rave"],
  [23, "Portal Pulse",      "rave"],
  [24, "Spectrum Sparks",   "rave"],
  [25, "Hyper Strobe",      "rave"],
  [26, "Cyber Tunnel",      "rave"],
  [27, "White Lightning",   "rave"],
  [28, "DnB Split",         "rave"],
  [29, "Rave Tiles",        "rave"],
  [30, "Pulse Train",       "rave"],
  [31, "Void Bloom",        "rave"]
];

// Internal diagnostic modes remain available from Audio Tune, but do not
// clutter the normal lighting controls. Legacy v21 IDs remain understood.
const diagnosticModes = [
  [1,"Space Pulse"],[2,"Beat Strobe"],[3,"Bass Comet"],[4,"Twinkle"],
  [5,"Spectrum"],[6,"Overall VU"],[7,"Glitter Kick"],[8,"Void Scanner"],
  [10,"Debug Chase"],[11,"Bass VU"],[12,"Mid VU"],[13,"Treble VU"],
  [14,"Beat Tracker"]
];
const allModeDefinitions = ledModes.concat(diagnosticModes);

const modeDefaults = {
  0:  {speed:0,   density:0,   sensitivity:0,   strobe:0,   twinkle:0},
  9:  {speed:0,   density:0,   sensitivity:0,   strobe:0,   twinkle:0},
  15: {speed:48,  density:118, sensitivity:0,   strobe:175, twinkle:24},
  16: {speed:62,  density:48,  sensitivity:0,   strobe:205, twinkle:170},
  17: {speed:118, density:76,  sensitivity:0,   strobe:185, twinkle:8},
  18: {speed:168, density:82,  sensitivity:0,   strobe:210, twinkle:0},
  19: {speed:178, density:112, sensitivity:0,   strobe:150, twinkle:20},
  20: {speed:118, density:105, sensitivity:168, strobe:175, twinkle:0},
  21: {speed:220, density:142, sensitivity:180, strobe:205, twinkle:16},
  22: {speed:194, density:78,  sensitivity:148, strobe:175, twinkle:38},
  23: {speed:130, density:108, sensitivity:158, strobe:185, twinkle:0},
  24: {speed:154, density:125, sensitivity:175, strobe:180, twinkle:190},
  25: {speed:232, density:74,  sensitivity:185, strobe:225, twinkle:0},
  26: {speed:168, density:98,  sensitivity:138, strobe:185, twinkle:0},
  27: {speed:178, density:82,  sensitivity:185, strobe:230, twinkle:12},
  28: {speed:198, density:124, sensitivity:168, strobe:185, twinkle:16},
  29: {speed:184, density:150, sensitivity:155, strobe:165, twinkle:0},
  30: {speed:205, density:88,  sensitivity:158, strobe:180, twinkle:0},
  31: {speed:120, density:82,  sensitivity:165, strobe:195, twinkle:92}
};

const colorPresets = [
  ["Red", 0, 255, "#ff2b2b"],
  ["Orange", 22, 255, "#ff8c1a"],
  ["Yellow", 42, 255, "#ffd83d"],
  ["Green", 96, 255, "#00d464"],
  ["Cyan", 128, 255, "#00d9ff"],
  ["Blue", 165, 255, "#2d74ff"],
  ["Purple", 190, 255, "#9d4dff"],
  ["Pink", 222, 255, "#ff4db8"],
  ["White", 0, 0, "#ffffff"]
];
const ledTypes = [
  [0, "WS2815 / WS2812-style RGB (RGB, 800kHz) — proven setup"],
  [1, "WS2811 RGB (RGB, 800kHz)"],
  [2, "WS2811 RGB (GRB, 800kHz)"],
  [3, "WS2811 older strips (RGB, 400kHz)"],
  [4, "SK6812 RGB (GRB, 800kHz)"],
  [5, "SK6812 RGBW (GRBW, 800kHz)"],
  [6, "SK6812 RGBW (RGBW, 800kHz)"]
];
const colorMaps = [
  [0, "RGB"],
  [1, "RBG"],
  [2, "GRB"],
  [3, "GBR"],
  [4, "BRG"],
  [5, "BGR"]
];
const MAX_STRANDS = 4;
const modeFields = ["enabled","mode","brightness","hue","saturation","speed","density","sensitivity","strobe","twinkle","direction"];
const allControlFields = ["brightness","speed","density","sensitivity","strobe","twinkle","direction"];
const hardwareFields = ["pin","ledCount","ledType"];
const strandFields = modeFields.concat(hardwareFields);
let loadedOnce = false;
let debounceTune = null;
let debounceLed = null;
let activeTab = "all";
let livePollTimer = null;
let livePollInFlight = false;
let tuningRequestInFlight = false;
let ledRequestInFlight = false;
let forceFullLedRequest = false;
let lastUiBeatPulse = null;
let uiBeatTimer = null;
const pendingTuningIds = new Set();
const pendingLedIds = new Set();

const tabNames = ["all","strand0","strand1","strand2","strand3","hardware","tuning"];

function showTab(name) {
  if (!tabNames.includes(name)) name = "all";
  activeTab = name;
  for (const tab of tabNames) {
    const page = document.getElementById("tab_" + tab);
    const button = document.getElementById("btn_" + tab);
    if (page) page.classList.toggle("active", tab === name);
    if (button) button.classList.toggle("active", tab === name);
  }
  const activeButton = document.getElementById("btn_" + name);
  if (activeButton) activeButton.scrollIntoView({behavior:"smooth", block:"nearest", inline:"center"});
  scheduleLivePoll(0);
}

function makeMeters(containerId) {
  const root = document.getElementById(containerId);
  if (!root) return;
  root.innerHTML = "";
  for (const name of meterNames) {
    root.innerHTML += `<div class="meterrow"><div>${name}</div><div class="meter"><div class="bar" id="${containerId}_${name}Bar"></div></div><div id="${containerId}_${name}Num">0</div></div>`;
  }
}

function setMeter(containerId, name, value) {
  const bar = document.getElementById(containerId + "_" + name + "Bar");
  const num = document.getElementById(containerId + "_" + name + "Num");
  if (!bar || !num) return;
  value = Math.max(0, Math.min(255, value || 0));
  bar.style.width = (value / 255 * 100) + "%";
  num.textContent = value;
}

function setControl(id, value) {
  const el = document.getElementById(id);
  const label = document.getElementById(id + "Val");
  if (!el) return;
  el.value = value;
  if (label) label.textContent = value;
}

function readControl(id) {
  const el = document.getElementById(id);
  return el ? el.value : "0";
}

function modeName(mode) {
  const found = allModeDefinitions.find(m => String(m[0]) === String(mode));
  return found ? found[1] : String(mode);
}

function modeGroupMarkup(prefix, clickFn) {
  const group = (name, title, note) => {
    const buttons = ledModes.filter(m => m[2] === name).map(m =>
      `<button type="button" class="choice-btn" id="${prefix}ModeBtn_${m[0]}" onclick="${clickFn}(${m[0]})">${m[1]}</button>`
    ).join("");
    return `<div class="mode-group-title">${title}</div><div class="mode-note">${note}</div><div class="mode-group-grid">${buttons}</div>`;
  };
  return group("general", "General / No Audio Required", "Solid Color and three dependable ambient effects are first. These modes work even with silence or the audio cable disconnected.") +
         group("rave", "Rave / EDM / Drum & Bass", "Sparse beams, chases, tunnels, bursts, and audio-energy effects designed to leave intentional black space between illuminated sections.");
}

function applyModeDefaultsToStrand(s, mode) {
  const d = modeDefaults[mode];
  const changed = [`s${s}_mode`];
  if (!d) return changed;
  for (const [field, value] of Object.entries(d)) {
    setControl(`s${s}_${field}`, value);
    changed.push(`s${s}_${field}`);
  }
  return changed;
}

function makeGlobalColorButtons() {
  const root = document.getElementById("globalColorButtons");
  root.innerHTML = "";
  for (const c of colorPresets) {
    if (c[0] === "White") continue;
    root.innerHTML += `<button type="button" class="color-btn" style="background:${c[3]};" onclick="setGlobalColor(${c[1]})">${c[0]}</button>`;
  }
}


function makeAllModeButtons() {
  const root = document.getElementById("allModeButtons");
  if (!root) return;
  root.innerHTML = modeGroupMarkup("all", "selectAllMode");
}

function makeAllColorButtons() {
  const root = document.getElementById("allColorButtons");
  if (!root) return;
  root.innerHTML = "";
  for (const c of colorPresets) {
    root.innerHTML += `<button type="button" class="color-btn" id="allColorBtn_${c[0]}" style="background:${c[3]}; color:${c[0] === 'White' ? '#111' : '#fff'}; text-shadow:${c[0] === 'White' ? 'none' : '0 1px 4px rgba(0,0,0,0.9)'};" onclick="setAllColor(${c[1]},${c[2]})">${c[0]}</button>`;
  }
}

function activeStrandCount() {
  return Math.max(1, Math.min(MAX_STRANDS, parseInt(readControl("activeStrands")) || 1));
}

function makeColorMapButtons() {
  const root = document.getElementById("colorMapButtons");
  if (!root) return;
  root.innerHTML = "";
  for (const m of colorMaps) {
    root.innerHTML += `<button type="button" class="choice-btn" id="colorMapBtn_${m[0]}" onclick="setColorMap(${m[0]})">${m[1]}</button>`;
  }
}

function refreshColorMapButtons() {
  const active = parseInt(readControl("colorMap")) || 0;
  for (const m of colorMaps) {
    const el = document.getElementById(`colorMapBtn_${m[0]}`);
    if (el) el.classList.toggle("active", active === m[0]);
  }
}

function setColorMap(v) {
  setControl("colorMap", v);
  refreshColorMapButtons();
  sendLedDebounced(["colorMap"]);
}

function setCalibrationTest(hue, saturation=255) {
  setControl("globalBrightness", 255);
  setControl("globalHue", 0);
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_mode`, 9);
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_brightness`, 255);
    setControl(`s${s}_hue`, hue);
    setControl(`s${s}_saturation`, saturation);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
}

function resetCalibration() {
  setControl("colorMap", 0);
  setControl("redScale", 255);
  setControl("greenScale", 255);
  setControl("blueScale", 255);
  setControl("gamma10", 10);
  refreshColorMapButtons();
  sendLedNow(true);
  showToast("Calibration reset to RGB / 255 / gamma 1.0");
}

function makeModeButtons(s) {
  const group = (name, title, note) => {
    const buttons = ledModes.filter(m => m[2] === name).map(m =>
      `<button type="button" class="choice-btn" id="s${s}_modeBtn_${m[0]}" onclick="selectMode(${s},${m[0]})">${m[1]}</button>`
    ).join("");
    return `<div class="mode-group-title">${title}</div><div class="mode-note">${note}</div><div class="mode-group-grid">${buttons}</div>`;
  };
  return group("general", "General / No Audio Required", "Reliable ambient modes that keep intentional dark space.") +
         group("rave", "Rave / EDM / Drum & Bass", "Electronic and audio-energy effects with beams, blocks, bursts, sparks, and black gaps.");
}

function makeColorButtons(s) {
  return colorPresets.map(c => `<button type="button" class="color-btn" id="s${s}_colorBtn_${c[0]}" style="background:${c[3]}; color:${c[0] === 'White' ? '#111' : '#fff'}; text-shadow:${c[0] === 'White' ? 'none' : '0 1px 4px rgba(0,0,0,0.9)'};" onclick="setStrandColor(${s},${c[1]},${c[2]})">${c[0]}</button>`).join("");
}

function makeModeStrandControls() {
  for (let s = 0; s < MAX_STRANDS; s++) {
    const root = document.getElementById(`strandControl_${s}`);
    if (!root) continue;
    root.innerHTML = `
      <div class="strand" id="modeCard_${s}">
        <div class="strand-heading">
          <h2>Strand ${s + 1} Controls</h2>
          <span id="strandState_${s}" class="pill">Active</span>
        </div>
        <p class="small">Only this strand is changed from this page. Hardware pin, LED count, type, and color calibration stay on the Hardware page.</p>
        <div class="toggle-row">
          <label>Enabled</label>
          <input id="s${s}_enabled" type="range" min="0" max="1" step="1">
        </div>
        <span id="s${s}_enabledVal" class="pill">0</span>
        <input id="s${s}_mode" class="hidden-input" type="hidden" value="0">
        <div class="section-title">Mode: <span id="s${s}_modeVal" class="pill">Off</span></div>
        <div>${makeModeButtons(s)}</div>
        <div class="section-title">Color</div>
        <div class="color-grid">${makeColorButtons(s)}</div>
        <div class="compact-controls grid">
          <label>Brightness</label><input id="s${s}_brightness" type="range" min="0" max="255" step="1"><span id="s${s}_brightnessVal"></span>
          <label>Hue</label><input id="s${s}_hue" type="range" min="0" max="255" step="1"><span id="s${s}_hueVal"></span>
          <label>Saturation</label><input id="s${s}_saturation" type="range" min="0" max="255" step="1"><span id="s${s}_saturationVal"></span>
          <label>Speed</label><input id="s${s}_speed" type="range" min="0" max="255" step="1"><span id="s${s}_speedVal"></span>
          <label>Density</label><input id="s${s}_density" type="range" min="0" max="255" step="1"><span id="s${s}_densityVal"></span>
          <label>Audio sensitivity</label><input id="s${s}_sensitivity" type="range" min="0" max="255" step="1"><span id="s${s}_sensitivityVal"></span>
          <label>Strobe/decay</label><input id="s${s}_strobe" type="range" min="0" max="255" step="1"><span id="s${s}_strobeVal"></span>
          <label>Twinkle/glitter</label><input id="s${s}_twinkle" type="range" min="0" max="255" step="1"><span id="s${s}_twinkleVal"></span>
          <label>Direction</label><input id="s${s}_direction" type="range" min="0" max="2" step="1"><span id="s${s}_directionVal"></span>
        </div>
        <div class="front-actions">
          <button onclick="sendLedNow(true)">Apply Strand ${s + 1}</button>
          <button onclick="saveAll()">Save Configuration</button>
        </div>
      </div>`;
  }
}

function makeHardwareStrandControls() {
  const root = document.getElementById("hardwareStrandControls");
  root.innerHTML = "";
  for (let s = 0; s < MAX_STRANDS; s++) {
    let typeOptions = ledTypes.map(t => `<option value="${t[0]}">${t[1]}</option>`).join("");
    root.innerHTML += `
      <div class="strand" id="hwCard_${s}">
        <h3>Strand ${s + 1} Hardware</h3>
        <div class="grid">
          <label>LED data GPIO</label><input id="s${s}_pin" type="number" min="0" max="48" step="1"><span id="s${s}_pinVal"></span>
          <label>LED count</label><input id="s${s}_ledCount" type="number" min="0" max="1000" step="1"><span id="s${s}_ledCountVal"></span>
          <label>LED type</label><select id="s${s}_ledType">${typeOptions}</select><span id="s${s}_ledTypeVal"></span>
        </div>
        <p class="small">Your proven WS2815/WS281x setup uses RGB at 800kHz. Other presets remain available for different strands.</p>
      </div>`;
  }
}

function refreshModeButtons(s) {
  const mode = readControl(`s${s}_mode`);
  const label = document.getElementById(`s${s}_modeVal`);
  if (label) label.textContent = modeName(mode);
  for (const m of ledModes) {
    const btn = document.getElementById(`s${s}_modeBtn_${m[0]}`);
    if (btn) btn.classList.toggle("active", String(m[0]) === String(mode));
  }
}

function refreshColorButtons(s) {
  const hue = parseInt(readControl(`s${s}_hue`)) || 0;
  const sat = parseInt(readControl(`s${s}_saturation`)) || 0;
  for (const c of colorPresets) {
    const btn = document.getElementById(`s${s}_colorBtn_${c[0]}`);
    if (!btn) continue;
    const closeHue = Math.abs(((hue - c[1] + 128 + 256) % 256) - 128) < 8;
    const active = (c[2] === 0 && sat < 20) || (c[2] > 0 && sat > 100 && closeHue);
    btn.classList.toggle("active", active);
  }
}


function refreshAllModeButtons() {
  const active = activeStrandCount();
  const mode0 = readControl("s0_mode");
  let allSame = true;
  for (let s = 1; s < active; s++) {
    if (readControl(`s${s}_mode`) !== mode0) allSame = false;
  }
  for (const m of ledModes) {
    const btn = document.getElementById(`allModeBtn_${m[0]}`);
    if (btn) btn.classList.toggle("active", allSame && String(m[0]) === String(mode0));
  }
}

function refreshAllColorButtons() {
  const hue = parseInt(readControl("s0_hue")) || 0;
  const sat = parseInt(readControl("s0_saturation")) || 0;
  const active = activeStrandCount();
  let allSame = true;
  for (let s = 1; s < active; s++) {
    if (readControl(`s${s}_hue`) !== readControl("s0_hue") || readControl(`s${s}_saturation`) !== readControl("s0_saturation")) allSame = false;
  }
  for (const c of colorPresets) {
    const btn = document.getElementById(`allColorBtn_${c[0]}`);
    if (!btn) continue;
    const closeHue = Math.abs(((hue - c[1] + 128 + 256) % 256) - 128) < 8;
    const activeColor = allSame && ((c[2] === 0 && sat < 20) || (c[2] > 0 && sat > 100 && closeHue));
    btn.classList.toggle("active", activeColor);
  }
}

function syncAllControlsFromStrand0() {
  for (const f of allControlFields) {
    setControl(`all_${f}`, readControl(`s0_${f}`));
  }
}

function refreshAllButtons() {
  for (let s = 0; s < MAX_STRANDS; s++) {
    refreshModeButtons(s);
    refreshColorButtons(s);
  }
  refreshAllModeButtons();
  refreshAllColorButtons();
  refreshStrandVisibility();
}

function refreshStrandVisibility() {
  const active = Math.max(1, Math.min(MAX_STRANDS, parseInt(readControl("activeStrands")) || 1));
  for (let s = 0; s < MAX_STRANDS; s++) {
    const isActive = s < active;
    const button = document.getElementById(`btn_strand${s}`);
    const state = document.getElementById(`strandState_${s}`);
    if (button) button.classList.toggle("inactive-strand", !isActive);
    if (state) {
      state.textContent = isActive ? "Active strand" : "Not in Active Strands";
      state.classList.toggle("warn", !isActive);
    }
  }
}

function selectMode(s, mode) {
  setControl(`s${s}_mode`, mode);
  const changed = applyModeDefaultsToStrand(s, mode);
  refreshModeButtons(s);
  if (s === 0) syncAllControlsFromStrand0();
  sendLedDebounced(changed);
}

function setStrandColor(s, hue, saturation) {
  setControl(`s${s}_hue`, hue);
  setControl(`s${s}_saturation`, saturation);
  refreshColorButtons(s);
  sendLedDebounced([`s${s}_hue`, `s${s}_saturation`]);
}


function selectAllMode(mode) {
  const active = activeStrandCount();
  const ids = [];
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, mode);
    ids.push(`s${s}_enabled`);
    ids.push(...applyModeDefaultsToStrand(s, mode));
    refreshModeButtons(s);
  }
  refreshAllModeButtons();
  syncAllControlsFromStrand0();
  sendLedDebounced(ids);
}

function setAllColor(hue, saturation) {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_hue`, hue);
    setControl(`s${s}_saturation`, saturation);
    refreshColorButtons(s);
  }
  refreshAllColorButtons();
  const ids = [];
  for (let s = 0; s < active; s++) ids.push(`s${s}_enabled`, `s${s}_hue`, `s${s}_saturation`);
  sendLedDebounced(ids);
}

function applyAllField(field, value) {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_${field}`, value);
    const label = document.getElementById(`s${s}_${field}Val`);
    if (label) label.textContent = value;
  }
  refreshAllButtons();
  const ids = [];
  for (let s = 0; s < active; s++) ids.push(`s${s}_${field}`);
  sendLedDebounced(ids);
}

function copyStrandOneToAll() {
  const active = activeStrandCount();
  for (let s = 1; s < active; s++) {
    for (const f of modeFields) {
      setControl(`s${s}_${f}`, readControl(`s0_${f}`));
    }
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
}

function setGlobalColor(hue) {
  setControl("globalHue", hue);
  sendLedDebounced(["globalHue"]);
}

async function applyCleanAudioBaseline() {
  const values = {
    shiftTo16:16,
    volumeGain:0.90, bassGain:1.15, midGain:1.05, trebleGain:1.45, lrGain:0.55,
    volumeFloor:44, bassFloor:70, midFloor:126, trebleFloor:83,
    attack:0.60, release:0.08, peakDecay:0.995,
    beatVolMin:35, beatBassMin:45,
    beatSensitivity:0.85, beatMinOnset:0.018,
    beatTempoAssist:1, beatPredictStrength:0.70, beatPredictToleranceMs:125,
    beatCooldownMs:210, beatHoldMs:85, beatMinBpm:70, beatMaxBpm:190,
    packetIntervalMs:33
  };
  for (const [id, value] of Object.entries(values)) setControl(id, value);
  updateSettingsText();
  const params = new URLSearchParams();
  addFullTuningConfig(params);
  try {
    const response = await fetch("/api/setTuning?" + params.toString(), {cache:"no-store"});
    if (!response.ok) throw new Error("baseline was not accepted");
    await fetch("/api/resetAgc", {cache:"no-store"});
    await fetch("/api/resetBeat", {cache:"no-store"});
    showToast("Clean audio baseline loaded; tune floors from silence first");
  } catch (e) {
    showToast("Baseline failed: " + e.message, true);
  }
}

function setFourMeterLayout() {
  const modes = [6, 11, 12, 13];
  const hues = [96, 8, 96, 175];
  const active = activeStrandCount();
  for (let s = 0; s < MAX_STRANDS; s++) {
    setControl(`s${s}_mode`, modes[s]);
    setControl(`s${s}_brightness`, 230);
    setControl(`s${s}_hue`, hues[s]);
    setControl(`s${s}_saturation`, 255);
    setControl(`s${s}_sensitivity`, 128);
    setControl(`s${s}_strobe`, 150);
    if (s < active) setControl(`s${s}_enabled`, 1);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("VU layout applied: Overall / Bass / Mid / Treble");
}

function setVuPreview(mode, label) {
  setControl("s0_enabled", 1);
  setControl("s0_mode", mode);
  setControl("s0_brightness", 230);
  setControl("s0_sensitivity", 128);
  setControl("s0_strobe", 150);
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast(`Strand 1 set to ${label}`);
}

function setAllOverallVu() {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, 6);
    setControl(`s${s}_brightness`, 230);
    setControl(`s${s}_sensitivity`, 128);
    setControl(`s${s}_strobe`, 150);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("Overall VU applied to all active strands");
}

function restoreSolidColor() {
  const active = activeStrandCount();
  for (let s = 0; s < active; s++) {
    setControl(`s${s}_enabled`, 1);
    setControl(`s${s}_mode`, 9);
    applyModeDefaultsToStrand(s, 9);
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  sendLedNow(true);
  showToast("Solid Color restored on all active strands");
}

function installControls() {
  for (const id of tuningIds) {
    const el = document.getElementById(id);
    if (!el) continue;
    el.addEventListener("input", () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      sendTuningDebounced(id);
      updateSettingsText();
    });
  }
  for (const id of ["activeStrands","maxLedsPerStrand","globalBrightness","globalHue","redScale","greenScale","blueScale","gamma10"]) {
    const el = document.getElementById(id);
    if (!el) continue;
    const update = () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      refreshAllButtons();
      sendLedDebounced([id]);
    };
    el.addEventListener("input", update);
    el.addEventListener("change", update);
  }
  for (const f of allControlFields) {
    const id = `all_${f}`;
    const el = document.getElementById(id);
    if (!el) continue;
    const update = () => {
      const label = document.getElementById(id + "Val");
      if (label) label.textContent = el.value;
      applyAllField(f, el.value);
    };
    el.addEventListener("input", update);
    el.addEventListener("change", update);
  }

  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) {
      const id = `s${s}_${f}`;
      const el = document.getElementById(id);
      if (!el) continue;
      const update = () => {
        const label = document.getElementById(id + "Val");
        if (label) label.textContent = el.value;
        if (f === "mode") refreshModeButtons(s);
        if (f === "hue" || f === "saturation") refreshColorButtons(s);
        if (s === 0 && allControlFields.includes(f)) syncAllControlsFromStrand0();
        refreshAllModeButtons();
        refreshAllColorButtons();
        sendLedDebounced([id]);
      };
      el.addEventListener("input", update);
      el.addEventListener("change", update);
    }
  }
}

function loadControlsFromState(j) {
  const c = j.cfg;
  for (const id of tuningIds) if (c[id] !== undefined) setControl(id, c[id]);
  const l = j.led;
  setControl("activeStrands", l.activeStrands);
  setControl("maxLedsPerStrand", l.maxLedsPerStrand);
  setControl("globalBrightness", l.globalBrightness);
  setControl("globalHue", l.globalHue);
  if (l.redScale !== undefined) setControl("redScale", l.redScale);
  if (l.greenScale !== undefined) setControl("greenScale", l.greenScale);
  if (l.blueScale !== undefined) setControl("blueScale", l.blueScale);
  if (l.gamma10 !== undefined) setControl("gamma10", l.gamma10);
  if (l.colorMap !== undefined) setControl("colorMap", l.colorMap);
  refreshColorMapButtons();
  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) {
      if (l.strand[s][f] !== undefined) setControl(`s${s}_${f}`, l.strand[s][f]);
    }
  }
  refreshAllButtons();
  syncAllControlsFromStrand0();
  updateSettingsText();
}

function sendTuningDebounced(id=null) {
  if (id) pendingTuningIds.add(id);
  clearTimeout(debounceTune);
  debounceTune = setTimeout(flushTuningQueue, 100);
}

function flushTuningQueue() {
  if (tuningRequestInFlight || pendingTuningIds.size === 0) return;
  const params = new URLSearchParams();
  for (const id of pendingTuningIds) params.set(id, readControl(id));
  pendingTuningIds.clear();
  tuningRequestInFlight = true;
  fetch("/api/setTuning?" + params.toString(), {cache:"no-store"})
    .catch(console.log)
    .finally(() => {
      tuningRequestInFlight = false;
      if (pendingTuningIds.size) debounceTune = setTimeout(flushTuningQueue, 30);
    });
}

function addFullTuningConfig(params) {
  for (const id of tuningIds) params.set(id, readControl(id));
}

function addFullLedConfig(params) {
  params.set("activeStrands", readControl("activeStrands"));
  params.set("maxLedsPerStrand", readControl("maxLedsPerStrand"));
  params.set("globalBrightness", readControl("globalBrightness"));
  params.set("globalHue", readControl("globalHue"));
  params.set("colorMap", readControl("colorMap"));
  params.set("redScale", readControl("redScale"));
  params.set("greenScale", readControl("greenScale"));
  params.set("blueScale", readControl("blueScale"));
  params.set("gamma10", readControl("gamma10"));
  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const f of strandFields) params.set(`s${s}_${f}`, readControl(`s${s}_${f}`));
  }
}

function sendLedDebounced(ids=[]) {
  if (!Array.isArray(ids)) ids = [ids];
  for (const id of ids) if (id) pendingLedIds.add(id);
  clearTimeout(debounceLed);
  debounceLed = setTimeout(flushLedQueue, 100);
}

function sendLedNow(full=true) {
  if (full) forceFullLedRequest = true;
  clearTimeout(debounceLed);
  flushLedQueue();
}

function flushLedQueue() {
  if (ledRequestInFlight) return;
  if (!forceFullLedRequest && pendingLedIds.size === 0) return;

  const params = new URLSearchParams();
  if (forceFullLedRequest) {
    addFullLedConfig(params);
  } else {
    for (const id of pendingLedIds) params.set(id, readControl(id));
  }
  pendingLedIds.clear();
  forceFullLedRequest = false;

  ledRequestInFlight = true;
  fetch("/api/setLed?" + params.toString(), {cache:"no-store"})
    .catch(console.log)
    .finally(() => {
      ledRequestInFlight = false;
      if (forceFullLedRequest || pendingLedIds.size) debounceLed = setTimeout(flushLedQueue, 20);
    });
}

function updateSettingsText() {
  const text =
`// Audio tuning values:
cfg.shiftTo16             = ${readControl("shiftTo16")};

cfg.volumeGain            = ${readControl("volumeGain")}f;
cfg.bassGain              = ${readControl("bassGain")}f;
cfg.midGain               = ${readControl("midGain")}f;
cfg.trebleGain            = ${readControl("trebleGain")}f;
cfg.lrGain                = ${readControl("lrGain")}f;

cfg.volumeFloor           = ${readControl("volumeFloor")}f;
cfg.bassFloor             = ${readControl("bassFloor")}f;
cfg.midFloor              = ${readControl("midFloor")}f;
cfg.trebleFloor           = ${readControl("trebleFloor")}f;

cfg.attack                = ${readControl("attack")}f;
cfg.release               = ${readControl("release")}f;
cfg.peakDecay             = ${readControl("peakDecay")}f;

cfg.beatVolMin            = ${readControl("beatVolMin")};
cfg.beatBassMin           = ${readControl("beatBassMin")};
cfg.beatSensitivity       = ${readControl("beatSensitivity")}f;
cfg.beatMinOnset          = ${readControl("beatMinOnset")}f;
cfg.beatTempoAssist       = ${readControl("beatTempoAssist")};
cfg.beatPredictStrength   = ${readControl("beatPredictStrength")}f;
cfg.beatPredictToleranceMs= ${readControl("beatPredictToleranceMs")};
cfg.beatCooldownMs        = ${readControl("beatCooldownMs")};
cfg.beatHoldMs            = ${readControl("beatHoldMs")};
cfg.beatMinBpm            = ${readControl("beatMinBpm")};
cfg.beatMaxBpm            = ${readControl("beatMaxBpm")};
cfg.packetIntervalMs      = ${readControl("packetIntervalMs")};`;
  document.getElementById("settingsText").textContent = text;
  return text;
}

function copyAudioSettings() {
  navigator.clipboard.writeText(updateSettingsText()).then(() => alert("Audio settings copied."));
}

let toastTimer = null;
function showToast(message, isError=false) {
  const el = document.getElementById("toast");
  if (!el) return;
  el.textContent = message;
  el.classList.toggle("error", isError);
  el.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove("show"), 2600);
}

function sleepMs(ms) { return new Promise(resolve => setTimeout(resolve, ms)); }

async function saveAll() {
  try {
    // Cancel pending debounce timers so the save cannot race an older slider request.
    clearTimeout(debounceLed);
    clearTimeout(debounceTune);
    pendingLedIds.clear();
    pendingTuningIds.clear();
    forceFullLedRequest = false;

    const waitStarted = Date.now();
    while ((ledRequestInFlight || tuningRequestInFlight) && Date.now() - waitStarted < 2500) {
      await sleepMs(25);
    }

    showToast("Applying current settings...");

    const ledParams = new URLSearchParams();
    addFullLedConfig(ledParams);
    const ledResponse = await fetch("/api/setLed?" + ledParams.toString(), {cache:"no-store"});
    if (!ledResponse.ok) throw new Error("LED settings were not accepted");

    const tuningParams = new URLSearchParams();
    addFullTuningConfig(tuningParams);
    const tuningResponse = await fetch("/api/setTuning?" + tuningParams.toString(), {cache:"no-store"});
    if (!tuningResponse.ok) throw new Error("Audio settings were not accepted");

    const saveResponse = await fetch("/api/save", {cache:"no-store"});
    const result = await saveResponse.json();
    if (!saveResponse.ok || !result.saved) throw new Error(result.error || "Flash verification failed");

    showToast(`Saved: ${result.activeStrands} active strand${result.activeStrands === 1 ? "" : "s"}`);
  } catch (e) {
    console.log(e);
    showToast("Save failed: " + e.message, true);
  }
}
function resetAgc() { fetch("/api/resetAgc"); }
function resetBeat() { fetch("/api/resetBeat"); }
function loadDefaults() { if (confirm("Load defaults for audio and LED settings?")) fetch("/api/defaults").then(() => { loadedOnce = false; setTimeout(updateState, 250); }); }

function setBeatLed(id, on) {
  const el = document.getElementById(id);
  if (!el) return;
  if (on) el.classList.add("on"); else el.classList.remove("on");
}

function applyLiveState(j) {
  for (const name of meterNames) setMeter("meters_tuning", name, j[name]);
  if (lastUiBeatPulse === null) {
    lastUiBeatPulse = j.beatPulse;
  } else if (j.beatPulse !== lastUiBeatPulse) {
    lastUiBeatPulse = j.beatPulse;
    setBeatLed("beatLightFinal", true);
    clearTimeout(uiBeatTimer);
    uiBeatTimer = setTimeout(() => setBeatLed("beatLightFinal", false), 140);
  } else if (!j.beat) {
    setBeatLed("beatLightFinal", false);
  }
  setBeatLed("beatLightCandidate", j.beatCandidate > 0);

  const rawEl = document.getElementById("rawText");
  if (rawEl) rawEl.textContent = `raw volume=${j.rawVolume.toFixed(1)} bass=${j.rawBass.toFixed(1)} mid=${j.rawMid.toFixed(1)} treble=${j.rawTreble.toFixed(1)} clip=${j.clip}`;

  const beatDebugEl = document.getElementById("beatDebugText");
  if (beatDebugEl) beatDebugEl.textContent = `FFT=${j.fftReady ? "ready" : "filling"} ${j.fftSize}pt peak=${j.fftPeakHz.toFixed(0)}Hz onset=${j.beatOnset.toFixed(3)} threshold=${j.beatThreshold.toFixed(3)} score=${j.beatScore.toFixed(2)} flux=${j.spectralFlux.toFixed(3)} low=${j.lowFlux.toFixed(3)} high=${j.highFlux.toFixed(3)} BPM=${j.beatBpm.toFixed(1)} confidence=${j.beatConfidence || 0} phase=${j.beatPhase || 0} age=${j.beatAge.toFixed(0)}ms candidate=${j.beatCandidate} tempoAssist=${j.beatTempoAssist}`;

  const map = {
    pps:j.pps, enqOk:j.enqOk, enqErr:j.enqErr, busySkip:j.busySkip,
    cbOk:j.cbOk, cbFail:j.cbFail, lastErr:j.lastErr,
    wifiRssi:`${j.wifiRssi} dBm`, wifiIp:j.wifiIp, wifiChannel:j.wifiChannel,
    freeHeap:j.freeHeap, fftUs:`${j.fftUs}/${j.fftAvgUs}`,
    analyzeUs:j.analyzeUs, httpUs:j.httpUs, loopUs:j.loopUs
  };
  for (const [id,value] of Object.entries(map)) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
  }

}

async function loadInitialState() {
  try {
    const r = await fetch("/api/state", {cache:"no-store"});
    const j = await r.json();
    loadControlsFromState(j);
    loadedOnce = true;
    applyLiveState(j);
  } catch (e) {
    console.log(e);
  }
}

function livePollDelay() {
  if (document.hidden) return 2500;
  if (activeTab === "tuning") return 200;
  if (activeTab === "hardware") return 600;
  return 1000;
}

function scheduleLivePoll(delay=livePollDelay()) {
  clearTimeout(livePollTimer);
  livePollTimer = setTimeout(pollLiveState, delay);
}

async function pollLiveState() {
  if (livePollInFlight) {
    scheduleLivePoll();
    return;
  }
  livePollInFlight = true;
  try {
    const r = await fetch("/api/live", {cache:"no-store"});
    const j = await r.json();
    applyLiveState(j);
  } catch (e) {
    console.log(e);
  } finally {
    livePollInFlight = false;
    scheduleLivePoll();
  }
}

// Backward-compatible helper used after defaults actions.
function updateState() {
  if (!loadedOnce) return loadInitialState();
  return pollLiveState();
}

document.addEventListener("visibilitychange", () => scheduleLivePoll(0));

makeMeters("meters_tuning");
makeGlobalColorButtons();
makeAllModeButtons();
makeAllColorButtons();
makeColorMapButtons();
makeModeStrandControls();
makeHardwareStrandControls();
// ---- Control source & strobe safety (Phase 13, /api/control) ----
let controlState = null;
function renderControl(c) {
  controlState = c;
  for (const s of ["auto", "artnet", "audio", "standalone"]) {
    const b = document.getElementById("src_" + s);
    if (b) b.classList.toggle("active", c.source === s);
  }
  const age = c.artnetAgeMs > 4000000000 ? "none yet" : (c.artnetAgeMs / 1000).toFixed(1) + " s ago";
  document.getElementById("controlStatus").textContent =
    "Active source: " + c.active.toUpperCase() + " (" + c.reason + "). Last Art-Net: " + age + ".";
  document.getElementById("ctlFallback").value = c.autoFallback;
  document.getElementById("ctlStandalone").value = c.standaloneLevel;
  document.getElementById("ctlStandaloneVal").textContent = c.standaloneLevel;
  document.getElementById("ctlStrobeHz").value = String(c.strobeHz);
  const sb = document.getElementById("ctlStrobeBtn");
  sb.textContent = c.strobeEnabled ? "Strobe: ON (tap to turn off)" : "Strobe: off (tap to enable)";
  sb.classList.toggle("active", c.strobeEnabled);
}
function controlCall(query) {
  return fetch("/api/control" + (query ? "?" + query : ""), {cache: "no-store"})
    .then(r => r.json()).then(renderControl).catch(() => showToast("Control update failed", true));
}
function setSource(s) { controlCall("source=" + s); }
function toggleStrobe() {
  if (controlState && controlState.strobeEnabled) { controlCall("strobe=0"); return; }
  if (confirm("Enable strobe? Flashing light can trigger seizures in photosensitive people. Max rate stays capped at " +
              (controlState ? controlState.strobeHz : 3) + " flashes/s.")) controlCall("strobe=1");
}
controlCall("");
setInterval(() => { if (!document.hidden) controlCall(""); }, 2000);

installControls();
showTab("all");
loadInitialState().finally(() => scheduleLivePoll(0));
</script>
</body>
</html>
)rawliteral";
