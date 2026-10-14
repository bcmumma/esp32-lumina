"""Phase 10 gate: compare the S3's decoded Art-Net with what this PC receives.

Acts as an Art-Net node on the PC (answers ArtPoll, like artnet_listen.py) and, at the
same time, polls the S3's /api/artnet "recent" frames. Frames are paired by universe-0
sequence number (only within a few seconds, because the sequence wraps every 255 frames),
and CH1-12 must match exactly. SoundSwitch gives each node's copy of a frame its own number
from one shared counter, so offsets 0, +1 and -1 are tried and reported.

    .venv\\Scripts\\python.exe tools/testbench/artnet_compare.py --s3 <s3-ip> --seconds 30
"""
import argparse
import json
import os
import socket
import struct
import sys
import threading
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
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)  # never drop PC frames
    sock.bind(("", ARTNET_PORT))
    sock.settimeout(0.05)

    pc = []            # (pc_time, seq, ch12)
    s3 = {}            # (seq, s3_ms) -> (pc_time_seen, ch12)
    t0 = time.time()
    s3_errors = 0
    lock = threading.Lock()
    stop = threading.Event()

    def poll_s3():
        # Separate thread: a slow or failed HTTP request must never stall the UDP reader.
        nonlocal s3_errors
        while not stop.is_set():
            try:
                j = json.load(urllib.request.urlopen(f"http://{args.s3}/api/artnet?n=0", timeout=2))
                seen = time.time()
                with lock:
                    for r in j["recent"]:
                        s3.setdefault((r[0], r[1]), (seen, r[2:14]))
            except Exception:
                s3_errors += 1
            stop.wait(0.5)  # 64-frame ring at ~37 fps covers ~1.7 s

    poller = threading.Thread(target=poll_s3, daemon=True)
    poller.start()
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
                    if parsed and parsed[0] == args.universe:
                        pc.append((time.time(), parsed[1], list(parsed[3][:12])))
        except socket.timeout:
            pass
    stop.set()
    poller.join(timeout=3)
    sock.close()

    print(f"PC frames: {len(pc)}   S3 frames collected: {len(s3)}   S3 API errors: {s3_errors}")
    # SoundSwitch numbers every unicast copy from one shared counter, so the copy of a frame
    # sent to the S3 can carry the PC copy's sequence +/-1 (measured 2026-10-14). Try each
    # offset; the right one pairs every frame with identical values.
    results = {}
    for offset in (0, 1, -1):
        matched = mismatched = unpaired = lit = 0
        distinct = set()
        bad = []
        for (seq, _ms), (seen, ch) in s3.items():
            want = (seq - offset - 1) % 255 + 1 if seq else 0
            cands = [p for p in pc if p[1] == want and abs(p[0] - seen) <= PAIR_WINDOW_S]
            if not cands:
                unpaired += 1
                continue
            best = min(cands, key=lambda p: abs(p[0] - seen))
            if best[2] == ch:
                matched += 1
                if any(ch):
                    lit += 1
                distinct.add(tuple(ch))
            else:
                mismatched += 1
                if len(bad) < 3:
                    bad.append((seq, ch, best[2]))
        results[offset] = (matched, mismatched, unpaired, bad, lit, len(distinct))
        print(f"offset S3=PC{offset:+d}: paired {matched + mismatched}, identical CH1-12 {matched} ({lit} lit, {len(distinct)} distinct), different {mismatched}, unpaired {unpaired}")
    # Per-frame check: SoundSwitch sends each frame to every node from one shared counter, so the
    # PC copy of an S3 frame is at seq-1 or seq+1 (either wrap style). With two nodes each node's
    # numbers repeat every ~3.5 s, so pair by arrival time: S3 ms mapped onto the PC clock.
    def neighbours(seq):
        out = set()
        for off in (-1, 1):
            out.add((seq + off) % 256)
            if seq:
                out.add((seq + off - 1) % 255 + 1)
        return out

    s3_frames = sorted(((seq, ms, ch) for (seq, ms), (_seen, ch) in s3.items()), key=lambda f: f[1])
    by_seq = {}
    for p_time, p_seq, p_ch in pc:
        by_seq.setdefault(p_seq, []).append((p_time, p_ch))
    # Clock offset from unambiguous matches (unique lit values), median of pc_time - s3_ms/1000.
    deltas = []
    for seq, ms, ch in s3_frames:
        if not any(ch):
            continue
        for n in neighbours(seq):
            for p_time, p_ch in by_seq.get(n, []):
                if p_ch == ch:
                    deltas.append(p_time - ms / 1000.0)
    if not deltas:
        lit_s3 = sum(1 for _s, _m, ch in s3_frames if any(ch))
        print(f"no lit frame matched a PC copy (S3 lit frames: {lit_s3}); cannot align clocks")
        verdict_override = "FAIL" if lit_s3 else "INCONCLUSIVE (no lit frames; is the show producing light?)"
        either = neither = lit = 0
        distinct = set()
    else:
        verdict_override = None
        deltas.sort()
        delta = deltas[len(deltas) // 2]
        either = neither = lit = no_partner = 0
        distinct = set()
        bad = []
        for seq, ms, ch in s3_frames:
            expect = ms / 1000.0 + delta
            cands = [(abs(p_time - expect), p_ch) for n in neighbours(seq) for p_time, p_ch in by_seq.get(n, [])
                     if abs(p_time - expect) <= 0.3]
            if not cands:
                no_partner += 1
                continue
            if any(p_ch == ch for _d, p_ch in cands):
                either += 1
                lit += 1 if any(ch) else 0
                distinct.add(tuple(ch))
            else:
                neither += 1
                if len(bad) < 5:
                    bad.append((seq, ch, min(cands)[1]))
        print(f"time-aligned per-frame: matches the PC copy {either} ({lit} lit, {len(distinct)} distinct), "
              f"matches neither {neither}, no PC frame within 0.3 s {no_partner}")
        for seq, ch, other in bad:
            print(f"  NO MATCH seq={seq}: S3={ch} nearest PC={other}")
    if verdict_override:
        print(f"RESULT: {verdict_override}")
        return
    if neither:
        verdict = "FAIL"
    elif lit < 20 or len(distinct) < 3:
        verdict = "INCONCLUSIVE (too few lit/varied frames; is the show producing light?)"
    else:
        verdict = "PASS"
    print(f"RESULT: {verdict} ({either} S3 frames identical to the PC copy of the same send, {neither} not)")


if __name__ == "__main__":
    main()
