import argparse
import json
import time
import urllib.parse
import urllib.request
import wave
from pathlib import Path

import serial

SAMPLE_RATE = 48000
CHANNELS = 2
SAMPLE_WIDTH_BYTES = 2
FRAME_BYTES = CHANNELS * SAMPLE_WIDTH_BYTES
BYTES_PER_SECOND = SAMPLE_RATE * FRAME_BYTES
START_MARKER = b"PCM1808_RAW16_STEREO_48K_START\n"


def http_json(base_url: str, path: str, timeout: float = 2.0):
    url = base_url.rstrip("/") + path
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def http_get(base_url: str, path: str, timeout: float = 2.0):
    url = base_url.rstrip("/") + path
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return r.read()


def ack(base_url: str, status: str):
    qs = urllib.parse.urlencode({"status": status})
    try:
        http_get(base_url, "/api/rec/ack?" + qs)
    except Exception:
        pass


def wait_for_marker(ser: serial.Serial, timeout_s: float = 10.0):
    deadline = time.monotonic() + timeout_s
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            idx = bytes(buf).find(START_MARKER)
            if idx >= 0:
                return bytes(buf[idx + len(START_MARKER):])
            if len(buf) > 8192:
                del buf[:-8192]
        else:
            time.sleep(0.01)
    preview = bytes(buf[-400:]).decode("utf-8", errors="replace")
    raise TimeoutError(f"Start marker not found. Last serial text:\n{preview}")


def record_from_serial(base_url: str, port: str, baud: int, seconds: int, filename: str, stop_id_at_start: int):
    out_path = Path(filename).expanduser()
    if out_path.parent and str(out_path.parent) != ".":
        out_path.parent.mkdir(parents=True, exist_ok=True)

    total_bytes = int(seconds * BYTES_PER_SECOND)
    total_bytes -= total_bytes % FRAME_BYTES

    print(f"Opening {port} at {baud} baud")
    ser = serial.Serial(port, baud, timeout=0.25, write_timeout=1)
    try:
        time.sleep(1.5)
        ser.reset_input_buffer()
        ser.write(b"r")
        ser.flush()

        initial = wait_for_marker(ser)
        ack(base_url, "recording")
        print(f"Recording {seconds}s -> {out_path}")

        bytes_done = 0
        carry = bytearray(initial)
        last_poll = time.monotonic()

        with wave.open(str(out_path), "wb") as wav:
            wav.setnchannels(CHANNELS)
            wav.setsampwidth(SAMPLE_WIDTH_BYTES)
            wav.setframerate(SAMPLE_RATE)

            while bytes_done < total_bytes:
                now = time.monotonic()
                if now - last_poll > 0.4:
                    try:
                        state = http_json(base_url, "/api/state", timeout=1.0)
                        if int(state["rec"]["stopId"]) != stop_id_at_start:
                            print("Stop requested from web UI")
                            break
                    except Exception:
                        pass
                    last_poll = now

                if carry:
                    usable = len(carry) - (len(carry) % FRAME_BYTES)
                    if usable:
                        data = bytes(carry[:usable])
                        del carry[:usable]
                        remaining = total_bytes - bytes_done
                        if len(data) > remaining:
                            data = data[:remaining]
                            data = data[:len(data) - (len(data) % FRAME_BYTES)]
                        wav.writeframesraw(data)
                        bytes_done += len(data)
                        continue

                want = min(8192, total_bytes - bytes_done)
                chunk = ser.read(want)
                if chunk:
                    carry.extend(chunk)

                sec_done = bytes_done / BYTES_PER_SECOND
                print(f"\r{sec_done:6.2f} / {seconds:.2f} sec", end="", flush=True)

        print()
        try:
            ser.write(b"q")
            ser.flush()
        except Exception:
            pass

        ack(base_url, f"saved:{out_path.name}")
        print(f"Saved {out_path}")
    finally:
        ser.close()


def main():
    parser = argparse.ArgumentParser(description="Background recorder agent for the ESP32 PCM1808 web Recorder tab.")
    parser.add_argument("esp32_url", help="ESP32 base URL, for example http://192.168.30.200")
    parser.add_argument("--poll", type=float, default=0.75, help="Polling interval seconds")
    args = parser.parse_args()

    last_cmd_id = None
    print(f"Polling {args.esp32_url}/api/state")
    print("Use the ESP32 web page Recorder tab to queue start/stop commands.")

    while True:
        try:
            state = http_json(args.esp32_url, "/api/state")
            rec = state["rec"]
            cmd_id = int(rec["cmdId"])
            stop_id = int(rec["stopId"])

            if last_cmd_id is None:
                last_cmd_id = cmd_id

            if cmd_id != last_cmd_id:
                last_cmd_id = cmd_id
                filename = rec.get("filename", "pcm1808_take.wav")
                port = rec.get("port", "COM4")
                baud = int(rec.get("baud", 2000000))
                seconds = int(rec.get("seconds", 10))
                try:
                    record_from_serial(args.esp32_url, port, baud, seconds, filename, stop_id)
                except Exception as e:
                    print(f"Recording failed: {e}")
                    ack(args.esp32_url, "error")

        except KeyboardInterrupt:
            print("Exiting")
            break
        except Exception as e:
            print(f"Poll error: {e}")

        time.sleep(args.poll)


if __name__ == "__main__":
    main()
