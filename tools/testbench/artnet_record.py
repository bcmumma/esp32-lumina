"""Record what SoundSwitch sends on each channel (through the S3) and summarise it.

Polls the S3's /api/artnet (latest frame) about 25 times a second, keeps each new frame,
writes them to a CSV in captures/, and prints one line per channel:
  range, how many distinct values, changes per second, and the strongest repeat period
  (in ms and as BPM if it looks beat-like). Channels that never move are listed as idle.

    python tools/testbench/artnet_record.py --s3 <ip> --channels 32 --secs 60 [--label probe-track1]

Use it to see which fixture functions SoundSwitch's AutoScript actually animates.
"""
import argparse
import csv
import json
import os
import statistics
import time
import urllib.request


def autocorr_period(values, times, lo_ms=150, hi_ms=2000):
    """Strongest repeat period of a channel, from resampled autocorrelation (None if flat)."""
    if len(values) < 40 or max(values) == min(values):
        return None, 0.0
    step = 25.0  # resample to 25 ms
    t0, t1 = times[0], times[-1]
    grid, j = [], 0
    t = t0
    while t <= t1:
        while j + 1 < len(times) and times[j + 1] <= t:
            j += 1
        grid.append(values[j])
        t += step / 1000.0
    m = statistics.mean(grid)
    x = [v - m for v in grid]
    var = sum(v * v for v in x)
    if var == 0:
        return None, 0.0
    # The first real repeat: after the autocorrelation has dropped below zero, take its highest
    # peak. (Slow, smooth channels correlate strongly at short lags without repeating.)
    rs = []
    for lag in range(1, min(int(hi_ms / step), len(x) // 2)):
        rs.append(sum(x[i] * x[i + lag] for i in range(len(x) - lag)) / var)
    crossed, best_lag, best_r = False, None, 0.0
    for i, r in enumerate(rs, start=1):
        if r < 0:
            crossed = True
        elif crossed and i * step >= lo_ms and r > best_r:
            best_lag, best_r = i, r
    return (best_lag * step if best_lag else None), best_r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--s3", required=True)
    ap.add_argument("--channels", type=int, default=32)
    ap.add_argument("--secs", type=float, default=60)
    ap.add_argument("--label", default="record")
    a = ap.parse_args()

    os.makedirs("captures", exist_ok=True)
    path = os.path.join("captures", f"artnet-{a.label}-{time.strftime('%Y%m%d-%H%M%S')}.csv")
    rows, last_seq, t0 = [], None, time.time()
    while time.time() - t0 < a.secs:
        try:
            j = json.load(urllib.request.urlopen(f"http://{a.s3}/api/artnet?n={a.channels}", timeout=2))
        except Exception:
            time.sleep(0.1)
            continue
        u0 = next((u for u in j.get("universes", []) if u.get("u") == 0), {})
        if u0.get("seq") != last_seq and len(j.get("ch", [])) >= a.channels:
            last_seq = u0.get("seq")
            rows.append((time.time() - t0, j["ch"][:a.channels]))
        time.sleep(0.03)

    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t"] + [f"ch{i + 1}" for i in range(a.channels)])
        for t, ch in rows:
            w.writerow([f"{t:.3f}"] + ch)
    print(f"{len(rows)} frames in {a.secs:.0f} s -> {path}")
    if len(rows) < 2:
        return
    times = [t for t, _ in rows]
    dur = times[-1] - times[0]
    idle = []
    print(f"{'ch':>4} {'min':>4} {'max':>4} {'distinct':>8} {'chg/s':>6} {'period':>8} {'~BPM':>5} {'r':>5}")
    for c in range(a.channels):
        v = [ch[c] for _, ch in rows]
        if max(v) == min(v):
            idle.append(f"{c + 1}({v[0]})")
            continue
        changes = sum(1 for p, q in zip(v, v[1:]) if p != q)
        per, r = autocorr_period(v, times)
        bpm = f"{60000 / per:5.0f}" if per and r > 0.3 else "    -"
        per_s = f"{per:6.0f}ms" if per and r > 0.3 else "       -"
        print(f"{c + 1:>4} {min(v):>4} {max(v):>4} {len(set(v)):>8} {changes / dur:>6.1f} {per_s:>8} {bpm} {r:5.2f}")
    if idle:
        print("idle (value):", ", ".join(idle))


if __name__ == "__main__":
    main()
