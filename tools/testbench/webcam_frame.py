"""Grab webcam frames of the strips into captures/ (gitignored: frames can show the room).

Needs the repo venv: .venv\\Scripts\\python.exe -m pip install opencv-python-headless pyserial

    .venv\\Scripts\\python.exe tools/testbench/webcam_frame.py                 # one frame
    .venv\\Scripts\\python.exe tools/testbench/webcam_frame.py --count 5 --every 0.5
    .venv\\Scripts\\python.exe tools/testbench/webcam_frame.py --camera 1 --exposure -6
"""
import argparse
import os
import time

import cv2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--camera", type=int, default=0)
    ap.add_argument("--count", type=int, default=1)
    ap.add_argument("--every", type=float, default=0.5, help="seconds between frames")
    ap.add_argument("--exposure", type=float, default=None, help="manual exposure (DirectShow log2 seconds, e.g. -6)")
    ap.add_argument("--out", default="captures/webcam")
    ap.add_argument("--prefix", default="frame")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    cam = cv2.VideoCapture(args.camera, cv2.CAP_DSHOW)
    if not cam.isOpened():
        raise SystemExit(f"camera {args.camera} did not open")
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    if args.exposure is not None:
        cam.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)  # DirectShow: 0.25 = manual
        cam.set(cv2.CAP_PROP_EXPOSURE, args.exposure)

    for _ in range(15):  # let auto exposure and white balance settle
        cam.read()

    stamp = time.strftime("%Y%m%d-%H%M%S")
    for i in range(args.count):
        ok, frame = cam.read()
        if not ok:
            raise SystemExit("frame read failed")
        path = os.path.join(args.out, f"{args.prefix}-{stamp}-{i:02d}.jpg")
        cv2.imwrite(path, frame)
        h, w = frame.shape[:2]
        print(f"t={time.time():.2f} saved {path} {w}x{h}", flush=True)
        if i + 1 < args.count:
            time.sleep(args.every)
    cam.release()


if __name__ == "__main__":
    main()
