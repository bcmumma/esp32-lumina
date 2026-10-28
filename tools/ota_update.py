"""Update a room receiver's firmware over Wi-Fi, no USB (Phase 16).

    python tools/ota_update.py --s3 <S3 ip> --room 1          # builds `receiver`, uploads to room 1
    python tools/ota_update.py --s3 <S3 ip> --room 2          # builds `receiver_room2`, uploads to room 2
    python tools/ota_update.py --s3 <S3 ip> --room 1 --no-build

Steps: build the room's firmware with PlatformIO; ask the S3 to put that room into update mode
(/api/control?update=N; the receiver joins the home Wi-Fi and reports its address in its heartbeat);
upload firmware.bin to http://<receiver>/update with the password from the gitignored
include/secrets.h (LUMINA_OTA_PASSWORD); wait until the room checks in again after its restart.
Receivers need the Phase 16 firmware (flashed once over USB) before this works.
"""
import argparse
import base64
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIO = os.path.join(os.environ.get("USERPROFILE", ""), ".platformio", "penv", "Scripts", "pio.exe")


def ota_password():
    path = os.path.join(ROOT, "include", "secrets.h")
    m = re.search(r'#define\s+LUMINA_OTA_PASSWORD\s+"([^"]*)"', open(path, encoding="utf-8").read())
    if not m:
        sys.exit("include/secrets.h has no LUMINA_OTA_PASSWORD")
    return m.group(1)


def rooms(s3):
    c = json.load(urllib.request.urlopen(f"http://{s3}/api/control", timeout=8))
    return {r["room"]: r for r in c.get("rooms", [])}


def upload(ip, firmware, password):
    data = open(firmware, "rb").read()
    boundary = uuid.uuid4().hex
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"update\"; filename=\"firmware.bin\"\r\n"
            f"Content-Type: application/octet-stream\r\n\r\n").encode() + data + f"\r\n--{boundary}--\r\n".encode()
    req = urllib.request.Request(f"http://{ip}/update", data=body, method="POST")
    req.add_header("Content-Type", f"multipart/form-data; boundary={boundary}")
    req.add_header("Authorization", "Basic " + base64.b64encode(f"lumina:{password}".encode()).decode())
    # Arduino-ESP32 3.x HTTPUpdateServer rejects uploads whose Origin isn't http://<Host> (CSRF check);
    # a browser form sends it automatically, so send the same.
    req.add_header("Origin", f"http://{ip}")
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.status, r.read().decode(errors="replace").strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--s3", required=True)
    ap.add_argument("--room", type=int, choices=[1, 2], required=True)
    ap.add_argument("--env")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    env = a.env or ("receiver" if a.room == 1 else "receiver_room2")
    firmware = os.path.join(ROOT, ".pio", "build", env, "firmware.bin")
    password = ota_password()

    if not a.no_build:
        print(f"building {env} ...")
        r = subprocess.run([PIO, "run", "-e", env], cwd=ROOT, capture_output=True, text=True)
        if "SUCCESS" not in r.stdout:
            sys.exit(r.stdout[-2000:] + r.stderr[-2000:])
    print(f"firmware: {os.path.getsize(firmware):,} bytes")

    r = rooms(a.s3).get(a.room, {})
    if not r.get("online"):
        sys.exit(f"room {a.room} is not online (no heartbeat); is it powered and running Phase 16 firmware?")
    before_uptime = r.get("uptimeS", 0)

    print(f"asking room {a.room} to join Wi-Fi ...")
    urllib.request.urlopen(f"http://{a.s3}/api/control?update={a.room}", timeout=8).read()
    ip, t0 = None, time.time()
    while time.time() - t0 < 90:
        r = rooms(a.s3).get(a.room, {})
        if r.get("updating") and r.get("ip"):
            ip = r["ip"]
            break
        time.sleep(1)
    if not ip:
        sys.exit(f"room {a.room} did not report a Wi-Fi address within 90 s (it stays in update mode up to 10 min)")
    print(f"room {a.room} ready at {ip}; uploading ...")

    status, text = upload(ip, firmware, password)
    print(f"upload: HTTP {status} {text[:80]}")
    if status != 200:
        sys.exit("upload failed")

    t0 = time.time()
    while time.time() - t0 < 90:
        time.sleep(2)
        try:
            r = rooms(a.s3).get(a.room, {})
        except Exception:
            continue
        if r.get("online") and not r.get("updating") and r.get("uptimeS", 1e9) < before_uptime:
            print(f"room {a.room} is back: uptime {r['uptimeS']} s, {r.get('fps')} fps, {r.get('lossPermille', 0) / 10:.1f}% loss. Done.")
            return
    sys.exit(f"room {a.room} did not check in again within 90 s after the upload")


if __name__ == "__main__":
    main()
