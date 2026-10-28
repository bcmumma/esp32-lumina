"""Phase 16 show soak: synthetic SoundSwitch show into the S3, both rooms' heartbeats logged.

Sends layout-L1 frames (6 x `5 Cell Dim` + the 5-channel Lumina FX fixture) to the S3 at 30 fps
in Auto: movement sections (Pan/Tilt sweep), busy sections (eighth-note Intensity pulses at
175 BPM), and every 10 minutes a 15 s stop, so the S3 falls back to Hybrid and Audio and returns.
Every 5 s it logs each room's heartbeat (online, fps, radio loss, restarts) and the S3's heap
and uptime to captures/show_soak_<time>.csv, then prints a pass/fail summary.

Pass: both rooms online in every sample, radio loss under 2%, no receiver or S3 restarts,
S3 free heap within 16 KB of where it started. SoundSwitch must be closed (no other Art-Net).
Settings it changes (source, fixture, brightness) are restored at the end.

    .venv\\Scripts\\python.exe tools/testbench/show_soak.py --s3 <s3-ip> --minutes 60
"""
import argparse
import csv
import json
import math
import os
import socket
import struct
import time
import urllib.request

EIGHTH = 60 / 175 / 2


def get(s3, path):
    return json.load(urllib.request.urlopen(f"http://{s3}{path}", timeout=5))


def frame(seq, t, fx, lit):
    d = bytearray(512)
    if lit:
        for f in range(6):  # 6 x (Dimmer + 5 RGB cells) = 16 channels
            b = f * 16
            d[b] = 255
            for c in range(5):
                hue = (f * 5 + c) / 30 + t * 0.1
                d[b + 1 + c * 3:b + 4 + c * 3] = bytes([int(40 + 40 * math.sin(6.28 * hue)), 0, 200])
        d[96:101] = bytes(fx)
    return b"Art-Net\x00" + struct.pack("<H", 0x5000) + bytes([0, 14, seq, 0, 0, 0]) + struct.pack(">H", 512) + bytes(d)


def show_fx(t):
    """32 s movement, 32 s busy, repeating."""
    if int(t / 32) % 2 == 0:
        return [255, 0, int(127 + 14 * math.sin(6.28 * t / 1.5)), int(127 + 28 * math.sin(6.28 * t / 1.5)), 0]
    return [255 if int(t / EIGHTH) % 2 == 0 else 0, 0, 127, 127, 0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--s3", required=True)
    ap.add_argument("--minutes", type=float, default=60)
    ap.add_argument("--brightness", type=int, default=50, help="global brightness during the soak (restored after)")
    a = ap.parse_args()

    before = get(a.s3, "/api/control")
    bright = get(a.s3, "/api/state")["led"]["globalBrightness"]
    get(a.s3, "/api/control?source=auto&fallback=audio&fixture=1&strobe=0")
    get(a.s3, f"/api/setLed?globalBrightness={a.brightness}")

    os.makedirs("captures", exist_ok=True)
    path = os.path.join("captures", time.strftime("show_soak_%Y%m%d_%H%M%S.csv"))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rows, seq, t0, next_log, next_frame = [], 1, time.time(), 0.0, 0.0
    end = a.minutes * 60
    print(f"soak {a.minutes:.0f} min -> {path}", flush=True)
    try:
        while (t := time.time() - t0) < end:
            if t >= next_frame:
                lit = (t % 600) < 585  # 15 s stop every 10 minutes
                if lit or int(t * 30) % 30 == 0:  # while stopped, one black frame a second
                    sock.sendto(frame(seq, t, show_fx(t), lit), (a.s3, 6454))
                    seq = seq % 255 + 1
                next_frame += 1 / 30
            if t >= next_log:
                next_log += 5
                try:
                    c, live = get(a.s3, "/api/control"), get(a.s3, "/api/live")
                except Exception as e:
                    rows.append({"t": round(t), "error": str(e)[:60]})
                    continue
                row = {"t": round(t), "active": c["active"], "s3Ms": live["ms"], "freeHeap": live["freeHeap"],
                       "minHeap": live["minHeap"], "s3Rssi": live["wifiRssi"]}
                for r in c.get("rooms", []):
                    n = r["room"]
                    row.update({f"r{n}online": int(bool(r["online"])), f"r{n}fps": r.get("fps"), f"r{n}good": r.get("good"),
                                f"r{n}lost": r.get("lost"), f"r{n}restarts": r.get("restarts"), f"r{n}rssi": r.get("rssi")})
                rows.append(row)
                if len(rows) % 60 == 1:
                    print(f"{t / 60:5.1f} min  {row['active']:<10} heap {row['freeHeap']}  "
                          f"r1 {row.get('r1online')} {row.get('r1fps')} fps  r2 {row.get('r2online')} {row.get('r2fps')} fps", flush=True)
            time.sleep(0.002)
    finally:
        get(a.s3, f"/api/control?source={before['source']}&fallback={before['autoFallback']}&strobe=0")
        code = before["fixture"]
        get(a.s3, f"/api/control?fixture={code - 200}" if code > 200 else f"/api/control?cells={code}")
        get(a.s3, f"/api/setLed?globalBrightness={bright}")
        keys = sorted({k for r in rows for k in r}, key=lambda k: (k != "t", k))
        with open(path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=keys)
            w.writeheader()
            w.writerows(rows)

    ok_rows = [r for r in rows if "error" not in r]
    print(f"\n{len(rows)} samples, {len(rows) - len(ok_rows)} S3 poll errors")
    passed = bool(ok_rows) and len(ok_rows) >= 0.98 * len(rows)
    for n in (1, 2):
        on = [r for r in ok_rows if f"r{n}online" in r]
        if not on:
            print(f"room {n}: no heartbeat seen  FAIL")
            passed = False
            continue
        offline = sum(1 for r in on if not r[f"r{n}online"])
        good = on[-1][f"r{n}good"] - on[0][f"r{n}good"]
        lost = on[-1][f"r{n}lost"] - on[0][f"r{n}lost"]
        restarts = on[-1][f"r{n}restarts"] - on[0][f"r{n}restarts"]
        loss = 100 * lost / max(1, good + lost)
        fps = [r[f"r{n}fps"] for r in on]
        ok = offline == 0 and restarts == 0 and loss < 2 and len(on) == len(ok_rows)
        passed &= ok
        print(f"room {n}: offline samples {offline}, restarts {restarts}, radio loss {loss:.2f}% ({lost}/{good + lost}), "
              f"fps min {min(fps)} median {sorted(fps)[len(fps) // 2]}  {'ok' if ok else 'FAIL'}")
    if ok_rows:
        s3_restart = any(b["s3Ms"] < a_["s3Ms"] for a_, b in zip(ok_rows, ok_rows[1:]))
        drift = ok_rows[0]["freeHeap"] - ok_rows[-1]["freeHeap"]
        modes = {}
        for r in ok_rows:
            modes[r["active"]] = modes.get(r["active"], 0) + 1
        ok = not s3_restart and drift < 16384
        passed &= ok
        print(f"S3: restart {s3_restart}, free heap {ok_rows[0]['freeHeap']} -> {ok_rows[-1]['freeHeap']} "
              f"(min ever {min(r['minHeap'] for r in ok_rows)}), active sources {modes}  {'ok' if ok else 'FAIL'}")
    print("PASS" if passed else "FAIL")


if __name__ == "__main__":
    main()
