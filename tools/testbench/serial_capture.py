"""Time-limited serial capture from one or more boards at once.

Resets each board through RTS (EN) and prints every line with a timestamp and port.
Close any open serial monitor first.

    python tools/testbench/serial_capture.py 10 COM<sender> COM<receiver>
    python tools/testbench/serial_capture.py 10 COM<sender> --no-reset
"""
import sys
import time

import serial


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.05
    s.dtr = False  # keep IO0 high so the board boots its app, not the bootloader
    s.rts = False
    s.open()
    return s


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    reset = "--no-reset" not in sys.argv
    secs = float(args[0])
    ports = {p: open_port(p) for p in args[1:]}

    if reset:
        for s in ports.values():
            s.rts = True
        time.sleep(0.1)
        for s in ports.values():
            s.rts = False
    else:
        for s in ports.values():
            s.reset_input_buffer()

    t0 = time.time()
    bufs = {p: b"" for p in ports}
    while time.time() - t0 < secs:
        for p, s in ports.items():
            bufs[p] += s.read(4096)
            while b"\n" in bufs[p]:
                line, bufs[p] = bufs[p].split(b"\n", 1)
                print(f"{time.time() - t0:6.1f}s {p} {line.decode('utf-8', 'replace').rstrip()}", flush=True)

    for s in ports.values():
        s.close()


if __name__ == "__main__":
    main()
