"""Play test patterns for the PCM1808 fallback (Phase 14) and report what the boards saw.

Segments: silence, a steady 60 Hz bass tone (expect no hits), house kicks at 128 BPM,
DnB kicks at 174 BPM, 1 kHz on the left channel only, then the right only, then silence.
Per segment it prints the S3's bass-hit count (serial `hits=`), the receiver's pulse
counter (serial `pulse=`), and the S3's left/right levels (from /api/state).

Plays through the Windows default output device (Modi -> Magni -> PCM1808). Keep the
Magni knob where the tone sweep was run, and stop any other audio first.

    python tools/testbench/audio_patterns.py --host <S3 ip> COM<sender> COM<receiver>
"""
import argparse
import json
import math
import os
import re
import struct
import tempfile
import threading
import time
import urllib.request
import wave
import winsound

import serial

RATE = 44100
PLAN = [  # (name, seconds)
    ("silence", 2.0),
    ("bass60", 5.0),
    ("house128", 6.0),
    ("dnb174", 6.0),
    ("left1k", 3.0),
    ("right1k", 3.0),
    ("silence", 2.0),
]
KICKS = {"house128": 128, "dnb174": 174}


def kick(t):
    """A synthetic kick: pitch falls 120 -> 50 Hz, about 150 ms decay."""
    f = 50 + 70 * math.exp(-t / 0.03)
    phase = 2 * math.pi * (50 * t + 70 * 0.03 * (1 - math.exp(-t / 0.03)))
    return math.sin(phase) * math.exp(-t / 0.15) if f else 0.0


def sample(name, t, amp):
    if name == "bass60":
        e = min(1.0, t / 0.05)
        return amp * e * math.sin(2 * math.pi * 60 * t), None
    if name in KICKS:
        period = 60.0 / KICKS[name]
        return amp * kick(t % period), None
    if name in ("left1k", "right1k"):
        v = amp * min(1.0, t / 0.05) * math.sin(2 * math.pi * 1000 * t)
        return (v, 0.0) if name == "left1k" else (0.0, v), "split"
    return 0.0, None


def write_wav(path, amp):
    frames = bytearray()
    for name, secs in PLAN:
        n = int(RATE * secs)
        for i in range(n):
            t = i / RATE
            fade = min(1.0, (n - i) / 400)
            v, split = sample(name, t, amp)
            if split:
                l, r = v
            else:
                l = r = v
            frames += struct.pack("<hh", int(32767 * l * fade), int(32767 * r * fade))
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


def reader(port, pattern, out, stop):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.05
    s.dtr = s.rts = False
    s.open()
    s.reset_input_buffer()
    buf = b""
    while not stop.is_set():
        buf += s.read(4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            m = re.search(pattern, line)
            if m:
                out.append((time.perf_counter(), int(m.group(1))))
    s.close()


def poller(host, out, stop):
    while not stop.is_set():
        try:
            st = json.load(urllib.request.urlopen(f"http://{host}/api/state", timeout=2))
            out.append((time.perf_counter(), st.get("left", 0), st.get("right", 0)))
        except Exception:
            pass
        time.sleep(0.25)


def value_at(series, t):
    before = [v for (ts, v) in series if ts <= t]
    return before[-1] if before else (series[0][1] if series else 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--amplitude", type=float, default=0.5)
    ap.add_argument("ports", nargs="+")
    a = ap.parse_args()

    path = os.path.join(tempfile.gettempdir(), "lumina_audio_patterns.wav")
    write_wav(path, a.amplitude)
    stop = threading.Event()
    s3hits, rxpulse, lr = [], [], []
    threads = [threading.Thread(target=reader, args=(a.ports[0], rb"hits=(\d+)", s3hits, stop), daemon=True),
               threading.Thread(target=poller, args=(a.host, lr, stop), daemon=True)]
    if len(a.ports) > 1:
        threads.append(threading.Thread(target=reader, args=(a.ports[1], rb"pulse=\s*(\d+)", rxpulse, stop), daemon=True))
    for th in threads:
        th.start()
    time.sleep(1.5)
    t0 = time.perf_counter()
    winsound.PlaySound(path, winsound.SND_FILENAME)
    time.sleep(1.5)
    stop.set()
    for th in threads:
        th.join(timeout=2)

    print(f"{'segment':10} {'secs':>4} {'S3 hits':>8} {'expected':>9} {'rx pulses':>10} {'left':>5} {'right':>5}")
    start = t0
    for name, secs in PLAN:
        end = start + secs
        # serial lines come once a second; shift by 0.5 s so each segment's own lines are used
        h = value_at(s3hits, end + 0.5) - value_at(s3hits, start + 0.5)
        p = (value_at(rxpulse, end + 0.5) - value_at(rxpulse, start + 0.5)) % 256 if rxpulse else "-"
        exp = f"~{int(secs * KICKS[name] / 60)}" if name in KICKS else "0"
        inner = [(l, r) for (ts, l, r) in lr if start + 0.6 < ts < end - 0.2]
        l = round(sum(x for x, _ in inner) / len(inner)) if inner else "-"
        r = round(sum(y for _, y in inner) / len(inner)) if inner else "-"
        print(f"{name:10} {secs:4.0f} {h:8} {exp:>9} {p!s:>10} {l!s:>5} {r!s:>5}")
        start = end


if __name__ == "__main__":
    main()
