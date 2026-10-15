"""Phase 11 network soak: log Art-Net, Wi-Fi, ESP-NOW, heap and ping, then summarize.

Reads only what the boards already report (no resets):
  - S3 /api/artnet (frames, gaps, maxIntervalMs, stalls; reset at start) and /api/live
    (RSSI, Wi-Fi state, ESP-NOW counters, heap, uptime), every 2 s
  - ping to the S3 (and optionally another host), once a second
  - the receiver's serial status lines (ESP-NOW good/lost/bad), without resetting it

For a valid Art-Net loss figure, the S3 must be SoundSwitch's only Art-Net node
(close artnet_listen.py / artnet_compare.py): SoundSwitch shares one sequence counter
across nodes, which makes per-node gap counts meaningless with several nodes.

    .venv\\Scripts\\python.exe tools/testbench/soak.py --s3 <s3-ip> --rx-port COM<n> --minutes 30
"""
import argparse
import csv
import json
import os
import re
import statistics
import subprocess
import threading
import time
import urllib.request

import serial


def get_json(url, timeout=1.5):
    return json.load(urllib.request.urlopen(url, timeout=timeout))


def ping_once(host):
    """Round-trip ms, or None on loss (Windows ping)."""
    try:
        out = subprocess.run(["ping", "-n", "1", "-w", "1000", host], capture_output=True, text=True, timeout=3).stdout
    except (subprocess.TimeoutExpired, OSError):
        return None
    m = re.search(r"time[=<](\d+)ms", out)
    return float(m.group(1)) if m else None


def pct(values, q):
    if not values:
        return None
    s = sorted(values)
    return s[min(len(s) - 1, int(round(q / 100 * (len(s) - 1))))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--s3", required=True)
    ap.add_argument("--rx-port", help="receiver serial port (read without reset)")
    ap.add_argument("--ping", action="append", default=[], help="extra host to ping (repeatable)")
    ap.add_argument("--minutes", type=float, default=30)
    ap.add_argument("--csv", default=None, help="default: captures/soak-<time>.csv")
    args = ap.parse_args()

    os.makedirs("captures", exist_ok=True)
    path = args.csv or time.strftime("captures/soak-%Y%m%d-%H%M%S.csv")
    duration = args.minutes * 60
    stop = threading.Event()

    pings = {h: [] for h in [args.s3] + args.ping}  # host -> list of rtt or None

    def pinger():
        while not stop.is_set():
            for h in pings:
                pings[h].append(ping_once(h))
            stop.wait(1.0)

    rx = {"first": None, "last": None, "lines": 0}

    def rx_reader():
        s = serial.Serial()
        s.port, s.baudrate, s.timeout = args.rx_port, 115200, 0.2
        s.dtr = False
        s.rts = False
        s.open()
        buf = b""
        while not stop.is_set():
            buf += s.read(4096)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                m = re.search(rb"rx good=(\d+) bad=(\d+) lost=(\d+)", line)
                if m:
                    v = tuple(int(x) for x in m.groups())
                    rx["first"] = rx["first"] or v
                    rx["last"] = v
                    rx["lines"] += 1
        s.close()

    threads = [threading.Thread(target=pinger, daemon=True)]
    if args.rx_port:
        threads.append(threading.Thread(target=rx_reader, daemon=True))
    for t in threads:
        t.start()

    base = f"http://{args.s3}"
    try:
        get_json(base + "/api/artnet?n=0&reset=1")
    except Exception as e:
        print(f"warning: could not reset S3 interval stats: {e}")

    rows, http_errors, reboots = [], 0, 0
    t0 = time.time()
    last_uptime = None
    print(f"Soak for {args.minutes:g} min -> {path}")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t", "uptime_ms", "wifi", "rssi", "pps", "enqErr", "cbFail", "heap", "minHeap",
                    "an_frames", "an_fps", "an_gaps", "an_ageMs", "an_maxIntervalMs", "an_stalls", "an_polls"])
        while time.time() - t0 < duration:
            tick = time.time()
            try:
                live = get_json(base + "/api/live")
                an = get_json(base + "/api/artnet?n=0")
                u0 = next((u for u in an["universes"] if u["u"] == 0), {})
                if last_uptime is not None and live["ms"] < last_uptime:
                    reboots += 1
                last_uptime = live["ms"]
                row = [round(tick - t0, 1), live["ms"], live["wifiConnected"], live["wifiRssi"], live["pps"], live["enqErr"],
                       live["cbFail"], live["freeHeap"], live["minHeap"], u0.get("frames"), u0.get("fps"), u0.get("gaps"),
                       u0.get("ageMs"), u0.get("maxIntervalMs"), u0.get("stalls"), an["polls"]]
                rows.append(row)
                w.writerow(row)
                f.flush()
            except Exception:
                http_errors += 1
            elapsed = time.time() - t0
            if int(elapsed) % 60 < 2 and rows:
                r = rows[-1]
                print(f"{elapsed / 60:5.1f} min  rssi={r[3]}  artnet fps={r[10]} gaps={r[11]} stalls={r[14]}  pps={r[4]}  heap={r[7]}")
            time.sleep(max(0.0, 2.0 - (time.time() - tick)))
    stop.set()
    for t in threads:
        t.join(timeout=3)

    print("\n=== Soak summary ===")
    print(f"duration {duration / 60:.1f} min, samples {len(rows)}, HTTP errors {http_errors}, S3 reboots {reboots}")
    if rows:
        a, z = rows[0], rows[-1]
        span = (z[1] - a[1]) / 1000 or 1
        frames = (z[9] or 0) - (a[9] or 0)
        gaps = (z[11] or 0) - (a[11] or 0)
        print(f"Art-Net u0: {frames} frames in {span:.0f} s = {frames / span:.1f} fps avg; "
              f"min 1-s fps {min((r[10] or 0) for r in rows)}; gaps {gaps} ({100 * gaps / max(1, frames + gaps):.2f}% of frames); "
              f"max interval {z[13]} ms; stalls>250ms {z[14]}; polls {z[15] - a[15]}")
        rssi = [r[3] for r in rows if r[2]]
        print(f"Wi-Fi: disconnected samples {sum(1 for r in rows if not r[2])}; RSSI min {min(rssi)} / median {statistics.median(rssi)} / max {max(rssi)} dBm")
        print(f"ESP-NOW (S3): pps median {statistics.median(r[4] for r in rows)}; send errors +{z[5] - a[5]}; callback fails +{z[6] - a[6]}")
        heaps = [r[7] for r in rows]
        print(f"heap: start {heaps[0]}, end {heaps[-1]}, min {min(heaps)}, drift {heaps[-1] - heaps[0]:+d} bytes; minHeap {z[8]}")
    if rx["first"] and rx["last"]:
        g = rx["last"][0] - rx["first"][0]
        lost = rx["last"][2] - rx["first"][2]
        bad = rx["last"][1] - rx["first"][1]
        print(f"ESP-NOW (receiver): good +{g}, lost +{lost} ({100 * lost / max(1, g + lost):.2f}%), bad +{bad}")
    for h, vals in pings.items():
        ok = [v for v in vals if v is not None]
        loss = 100 * (len(vals) - len(ok)) / max(1, len(vals))
        if ok:
            print(f"ping {h}: {len(vals)} sent, {loss:.1f}% loss, p50 {pct(ok, 50):.0f} ms, p95 {pct(ok, 95):.0f} ms, max {max(ok):.0f} ms")
        else:
            print(f"ping {h}: {len(vals)} sent, all lost")


if __name__ == "__main__":
    main()
