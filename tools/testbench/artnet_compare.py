"""Phase 10 gate: compare the S3's decoded Art-Net with what this PC receives.

Acts as an Art-Net node on the PC (answers ArtPoll, like artnet_listen.py) and, at the
same time, polls the S3's /api/artnet "recent" frames. Frames are paired by universe-0
sequence number (only within a few seconds, because the sequence wraps every 255 frames),
and CH1-12 must match exactly.

    .venv\\Scripts\\python.exe tools/testbench/artnet_compare.py --s3 <s3-ip> --seconds 30
"""
import argparse
import json
import os
import socket
import struct
import sys
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artnet_listen import ARTNET_ID, ARTNET_PORT, OP_DMX, OP_POLL, local_ip_towards, parse_dmx, poll_reply  # noqa: E402

PAIR_WINDOW_S = 3.0  # sequence repeats every ~7 s at 37 fps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--s3", required=True, help="S3 IP (its /api/artnet is polled)")
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--universe", type=int, default=0)
    ap.add_argument("--advertise", help="IP for this PC's ArtPollReply (default: same-subnet address)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", ARTNET_PORT))
    sock.settimeout(0.05)

    pc = []            # (pc_time, seq, ch12)
    s3 = {}            # (seq, s3_ms) -> (pc_time_seen, ch12)
    t0 = time.time()
    next_poll = t0
    s3_errors = 0
    print(f"Comparing universe {args.universe} for {args.seconds:g} s (PC node + S3 {args.s3}/api/artnet)...")
    while time.time() - t0 < args.seconds:
        try:
            data, (src_ip, _port) = sock.recvfrom(2048)
            if data[:8] == ARTNET_ID and len(data) >= 10:
                op = struct.unpack("<H", data[8:10])[0]
                if op == OP_POLL:
                    reply = poll_reply(args.advertise or local_ip_towards(src_ip), args.universe)
                    for dest in ((src_ip, ARTNET_PORT), ("255.255.255.255", ARTNET_PORT)):
                        try:
                            sock.sendto(reply, dest)
                        except OSError:
                            pass
                elif op == OP_DMX:
                    parsed = parse_dmx(data)
                    if parsed and parsed[0] == args.universe and parsed[1] != 0:
                        pc.append((time.time(), parsed[1], list(parsed[3][:12])))
        except socket.timeout:
            pass
        now = time.time()
        if now >= next_poll:
            next_poll = now + 0.5  # 64-frame ring at ~37 fps covers ~1.7 s
            try:
                j = json.load(urllib.request.urlopen(f"http://{args.s3}/api/artnet?n=0", timeout=2))
                for r in j["recent"]:
                    s3.setdefault((r[0], r[1]), (now, r[2:14]))
            except Exception:
                s3_errors += 1
    sock.close()

    matched = mismatched = s3_unpaired = 0
    examples = []
    for (seq, _ms), (seen, ch) in s3.items():
        candidates = [p for p in pc if p[1] == seq and abs(p[0] - seen) <= PAIR_WINDOW_S]
        if not candidates:
            s3_unpaired += 1
            continue
        best = min(candidates, key=lambda p: abs(p[0] - seen))
        if best[2] == ch:
            matched += 1
            if len(examples) < 3 and any(ch):
                examples.append((seq, ch))
        else:
            mismatched += 1
            if mismatched <= 5:
                print(f"MISMATCH seq={seq}: S3={ch} PC={best[2]}")
    print(f"PC frames: {len(pc)}   S3 frames collected: {len(s3)}   S3 API errors: {s3_errors}")
    print(f"paired by sequence: {matched + mismatched}   identical CH1-12: {matched}   different: {mismatched}   S3 frames with no PC partner: {s3_unpaired}")
    for seq, ch in examples:
        print(f"  e.g. seq={seq} CH1-12={ch} on both")
    verdict = "PASS" if matched > 0 and mismatched == 0 else "FAIL"
    print(f"RESULT: {verdict}")


if __name__ == "__main__":
    main()
