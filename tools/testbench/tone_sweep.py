"""Play a silence / 100 Hz / 1 kHz / 6 kHz / silence sweep and log board output during it.

Plays through the Windows default output device (Modi 3E DAC -> Magni amp -> PCM1808, no speakers).
Readings depend on the Magni volume knob, so keep it at the same position between runs.
Boards are not reset, so they keep running. Lines are tagged with the segment playing.

    python tools/testbench/tone_sweep.py COM<sender> COM<receiver>
"""
import math
import os
import struct
import sys
import tempfile
import threading
import time
import wave
import winsound

import serial

SAMPLE_RATE = 44100
AMPLITUDE = 0.25  # fraction of full scale
PLAN = [("silence", 0, 3), ("100Hz", 100, 3), ("1kHz", 1000, 3), ("6kHz", 6000, 3), ("silence", 0, 3)]


def write_wav(path):
    frames = bytearray()
    for _, freq, secs in PLAN:
        n = int(SAMPLE_RATE * secs)
        for i in range(n):
            env = min(1.0, i / 2000, (n - i) / 2000)  # short fade to avoid clicks
            v = int(32767 * AMPLITUDE * env * math.sin(2 * math.pi * freq * i / SAMPLE_RATE)) if freq else 0
            frames += struct.pack("<hh", v, v)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(bytes(frames))


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.05
    s.dtr = False
    s.rts = False  # no reset
    s.open()
    s.reset_input_buffer()
    return s


def main():
    wav = os.path.join(tempfile.gettempdir(), "lumina_tone_sweep.wav")
    write_wav(wav)
    ports = {p: open_port(p) for p in sys.argv[1:]}

    edges, total = [], 0
    for name, _, secs in PLAN:
        edges.append((total, total + secs, name))
        total += secs

    t0 = time.time()
    threading.Thread(target=lambda: winsound.PlaySound(wav, winsound.SND_FILENAME), daemon=True).start()
    bufs = {p: b"" for p in ports}
    while time.time() - t0 < total + 1:
        for p, s in ports.items():
            bufs[p] += s.read(4096)
            while b"\n" in bufs[p]:
                line, bufs[p] = bufs[p].split(b"\n", 1)
                t = time.time() - t0
                seg = next((n for a, b, n in edges if a <= t < b), "after")
                print(f"{t:5.1f}s [{seg:7}] {p} {line.decode('utf-8', 'replace').rstrip()}", flush=True)

    for s in ports.values():
        s.close()


if __name__ == "__main__":
    main()
