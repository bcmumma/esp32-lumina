"""Send a known Art-Net (ArtDmx) test pattern, for checking a receiver without SoundSwitch.

Pattern on CH1-12 (Lumina fixture: strand 1-4 RGB), stepping once a second:
  step 0: S1 red, S2 green, S3 blue, S4 white
  step 1: rotated by one strand, and so on.
The level is low by default (64) because Phase 10+ will drive real LEDs from this.

    .venv\\Scripts\\python.exe tools/testbench/artnet_send.py --to 127.0.0.1 --seconds 10
    .venv\\Scripts\\python.exe tools/testbench/artnet_send.py --to <s3-ip> --fps 40        # Phase 10+
    .venv\\Scripts\\python.exe tools/testbench/artnet_send.py --poll --to 127.0.0.1         # ArtPoll only
"""
import argparse
import socket
import struct
import time

ARTNET_PORT = 6454
ARTNET_ID = b"Art-Net\x00"


def art_dmx(universe, seq, dmx):
    if len(dmx) % 2:
        dmx = dmx + b"\x00"  # spec: even length
    return (ARTNET_ID + struct.pack("<H", 0x5000) + bytes([0, 14, seq, 0, universe & 0xFF, (universe >> 8) & 0x7F])
            + struct.pack(">H", len(dmx)) + dmx)


def art_poll():
    return ARTNET_ID + struct.pack("<H", 0x2000) + bytes([0, 14, 0x00, 0x00])


def pattern(step, level):
    colors = [(level, 0, 0), (0, level, 0), (0, 0, level), (level, level, level)]
    out = bytearray(512)
    for strand in range(4):
        r, g, b = colors[(strand + step) % 4]
        out[strand * 3:strand * 3 + 3] = bytes([r, g, b])
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--to", action="append", help="receiver IP or broadcast address; repeat to send the same frames to several")
    ap.add_argument("--universe", type=int, default=0)
    ap.add_argument("--fps", type=float, default=40)
    ap.add_argument("--seconds", type=float, default=10)
    ap.add_argument("--level", type=int, default=64, help="0-255 channel level for lit colors")
    ap.add_argument("--poll", action="store_true", help="send one ArtPoll and print any ArtPollReply")
    ap.add_argument("--lumina16", help="comma-separated fixed LUMINA-16 channel values (CH1..), e.g. 255,0,0,128,128,0,0,0,255,0,0")
    ap.add_argument("--address", type=int, default=1, help="1-based DMX address for --lumina16")
    args = ap.parse_args()
    targets = args.to or ["127.0.0.1"]

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    if args.poll:
        sock.bind(("", 0))
        sock.settimeout(2.0)
        sock.sendto(art_poll(), (targets[0], ARTNET_PORT))
        try:
            data, src = sock.recvfrom(1024)
            name = data[26:44].split(b"\x00")[0].decode(errors="replace")
            op = struct.unpack("<H", data[8:10])[0]
            print(f"reply op=0x{op:04X} len={len(data)} short name={name!r} from {src[0]}")
        except socket.timeout:
            print("no ArtPollReply within 2 s (replies go to port 6454; run this with the monitor stopped to test unicast replies)")
        return

    seq, t0, sent = 1, time.time(), 0
    period = 1.0 / args.fps
    print(f"Sending ArtDmx universe {args.universe} to {', '.join(targets)} (port {ARTNET_PORT}) at {args.fps:g} fps for {args.seconds:g} s")
    while time.time() - t0 < args.seconds:
        step = int(time.time() - t0)
        if args.lumina16:
            values = [max(0, min(255, int(v))) for v in args.lumina16.split(",")][:16]
            data = bytearray(512)
            data[args.address - 1:args.address - 1 + len(values)] = bytes(values)
            frame = art_dmx(args.universe, seq, bytes(data))
        else:
            frame = art_dmx(args.universe, seq, pattern(step, args.level))
        for to in targets:
            sock.sendto(frame, (to, ARTNET_PORT))
        seq = seq % 255 + 1
        sent += 1
        time.sleep(period)
    print(f"Sent {sent} frames.")


if __name__ == "__main__":
    main()
