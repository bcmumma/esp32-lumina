"""Drive one strand through off/red/green/blue/white over the sender API and check colours on camera.

Live settings only (nothing is saved); the strand's original settings are restored at the end.
Each colour frame is compared with the "off" frame, so room light cancels out.

    .venv\\Scripts\\python.exe tools/testbench/strand_colors.py --host <sender-ip> [--strand 0] [--brightness 80]
"""
import argparse
import json
import os
import time
import urllib.request

import cv2
import numpy as np

STEPS = [
    ("off", {"mode": 0}),
    ("red", {"mode": 9, "hue": 0, "saturation": 255}),
    ("green", {"hue": 85}),
    ("blue", {"hue": 170}),
    ("white", {"saturation": 0}),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--strand", type=int, default=0)
    ap.add_argument("--brightness", type=int, default=80, help="0-255; v2 applies gamma, so 80 is about 10%% power")
    ap.add_argument("--camera", type=int, default=0)
    ap.add_argument("--exposure", type=float, default=-7)
    ap.add_argument("--out", default="captures/webcam")
    args = ap.parse_args()

    base = f"http://{args.host}"
    get = lambda path: urllib.request.urlopen(base + path, timeout=5).read().decode()
    s = args.strand
    set_led = lambda fields: get("/api/setLed?" + "&".join(f"s{s}_{k}={v}" for k, v in fields.items()))

    orig = json.loads(get("/api/state"))["led"]["strand"][s]
    keep = {k: orig[k] for k in ("mode", "brightness", "hue", "saturation")}

    cam = cv2.VideoCapture(args.camera, cv2.CAP_DSHOW)
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cam.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
    cam.set(cv2.CAP_PROP_EXPOSURE, args.exposure)
    for _ in range(20):
        cam.read()

    os.makedirs(args.out, exist_ok=True)
    frames = {}
    try:
        for name, fields in STEPS:
            if name == "red":
                fields = dict(fields, brightness=args.brightness)
            set_led(fields)
            time.sleep(2.0)
            for _ in range(6):
                cam.read()
            ok, f = cam.read()
            if not ok:
                raise SystemExit("frame read failed")
            frames[name] = f.astype(np.float32)
            cv2.imwrite(os.path.join(args.out, f"strand{s}-{name}.jpg"), f)
    finally:
        cam.release()
        set_led(keep)
        print(f"restored strand {s} live settings {keep} (not saved)")

    off = frames["off"]
    for name in ("red", "green", "blue", "white"):
        d = np.clip(frames[name] - off, 0, None)
        lum = d.sum(2)
        b, g, r = d[lum >= np.percentile(lum, 99.5)].mean(0)
        total = r + g + b
        print(f"{name:6} lit-pixel share R={r / total:.2f} G={g / total:.2f} B={b / total:.2f}  (delta {total:.0f})")


if __name__ == "__main__":
    main()
