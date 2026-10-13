"""Art-Net monitor for the PC: shows what SoundSwitch (or any controller) sends.

- Listens on UDP 6454 and decodes ArtDmx: universe, frame rate, sequence gaps,
  and the first channels as Lumina strands (CH1-3 = strand 1 RGB, ...).
- Answers ArtPoll with an ArtPollReply so the PC shows up as an Art-Net node
  ("Lumina PC Monitor") in controllers that only send to discovered nodes.
  Use --no-reply to stay silent.

    .venv\\Scripts\\python.exe tools/testbench/artnet_listen.py
    .venv\\Scripts\\python.exe tools/testbench/artnet_listen.py --seconds 30 --csv captures/artnet-summary.csv

Windows may ask whether Python can use the network the first time: that's a
firewall change, so it's the owner's decision (Private networks only).
"""
import argparse
import csv
import socket
import struct
import time

ARTNET_PORT = 6454
ARTNET_ID = b"Art-Net\x00"
OP_POLL = 0x2000
OP_POLL_REPLY = 0x2100
OP_DMX = 0x5000


def local_ip_towards(peer_ip):
    """IP of the local interface that routes to peer_ip (no packets are sent)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((peer_ip, ARTNET_PORT))
        return s.getsockname()[0]
    finally:
        s.close()


def poll_reply(my_ip, universe=0):
    """ArtPollReply per the Art-Net 4 spec (239 bytes), advertising one DMX output port."""
    net = (universe >> 8) & 0x7F
    sub = (universe >> 4) & 0x0F
    uni = universe & 0x0F
    def text(s, n):
        return s.encode("ascii")[: n - 1].ljust(n, b"\x00")
    pkt = bytearray()
    pkt += ARTNET_ID
    pkt += struct.pack("<H", OP_POLL_REPLY)
    pkt += socket.inet_aton(my_ip)
    pkt += struct.pack("<H", ARTNET_PORT)
    pkt += bytes([0, 1])             # VersInfo (firmware 0.1)
    pkt += bytes([net, sub])         # NetSwitch, SubSwitch
    pkt += bytes([0x00, 0xFF])       # OEM: 0x00FF = unknown
    pkt += bytes([0])                # UBEA
    pkt += bytes([0xD0])             # Status1: indicators normal
    pkt += bytes([0, 0])             # ESTA manufacturer
    pkt += text("Lumina PC Monitor", 18)
    pkt += text("Lumina test bench Art-Net monitor (tools/testbench/artnet_listen.py)", 64)
    pkt += text("#0001 [0000] OK", 64)
    pkt += struct.pack(">H", 1)      # NumPorts
    pkt += bytes([0x80, 0, 0, 0])    # PortTypes: port 1 outputs DMX512 from Art-Net
    pkt += bytes([0, 0, 0, 0])       # GoodInput
    pkt += bytes([0x80, 0, 0, 0])    # GoodOutputA: data is being output
    pkt += bytes([0, 0, 0, 0])       # SwIn
    pkt += bytes([uni, 0, 0, 0])     # SwOut
    pkt += bytes([0, 0, 0])          # AcnPriority, SwMacro, SwRemote
    pkt += bytes([0, 0, 0])          # spare
    pkt += bytes([0x00])             # Style: StNode
    pkt += bytes(6)                  # MAC (not reported)
    pkt += socket.inet_aton(my_ip)   # BindIp
    pkt += bytes([1])                # BindIndex
    pkt += bytes([0x08])             # Status2: supports 15-bit Port-Address
    pkt += bytes([0, 0, 0, 0])       # GoodOutputB
    pkt += bytes([0])                # Status3
    pkt += bytes(6)                  # DefaultRespUID
    pkt += bytes(2) + bytes(2)       # User, RefreshRate
    pkt += bytes(11)                 # filler
    assert len(pkt) == 239, len(pkt)
    return bytes(pkt)


def parse_dmx(data):
    """Return (universe, sequence, physical, dmx_bytes) or None."""
    if len(data) < 18:
        return None
    seq, phys, subuni, net = data[12], data[13], data[14], data[15]
    length = struct.unpack(">H", data[16:18])[0]
    return (net << 8) | subuni, seq, phys, data[18:18 + length]


def strands(dmx, n):
    groups = []
    for i in range(0, min(n, len(dmx)), 3):
        rgb = tuple(dmx[i:i + 3])
        groups.append(f"S{i // 3 + 1}={','.join(str(v) for v in rgb):<11}")
    return " ".join(groups)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=0, help="stop after N seconds (0 = until Ctrl+C)")
    ap.add_argument("--channels", type=int, default=12, help="channels to show (default 12 = 4 strands x RGB)")
    ap.add_argument("--universe", type=int, default=0, help="universe advertised in ArtPollReply")
    ap.add_argument("--no-reply", action="store_true", help="don't answer ArtPoll")
    ap.add_argument("--bind", default="", help="local address to listen on (default all; 127.0.0.1 for the self-test)")
    ap.add_argument("--csv", help="write a one-line-per-second summary (keep it in captures/, which is gitignored)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind((args.bind, ARTNET_PORT))
    sock.settimeout(0.5)
    print(f"Listening for Art-Net on UDP {ARTNET_PORT} (ArtPoll replies {'off' if args.no_reply else 'on'}). Ctrl+C to stop.")

    writer = None
    if args.csv:
        f = open(args.csv, "w", newline="")
        writer = csv.writer(f)
        writer.writerow(["t", "universe", "frames_per_s", "seq_gaps", "length", "broadcast", "channels"])

    t0 = time.time()
    window_start = t0
    stats = {}  # universe -> dict
    polls = 0
    try:
        while not args.seconds or time.time() - t0 < args.seconds:
            try:
                data, (src_ip, src_port) = sock.recvfrom(2048)
            except socket.timeout:
                data = None
            if data and data[:8] == ARTNET_ID and len(data) >= 10:
                op = struct.unpack("<H", data[8:10])[0]
                if op == OP_POLL:
                    polls += 1
                    if not args.no_reply:
                        reply = poll_reply(local_ip_towards(src_ip), args.universe)
                        for dest in ((src_ip, ARTNET_PORT), ("255.255.255.255", ARTNET_PORT)):
                            try:
                                sock.sendto(reply, dest)
                            except OSError as e:  # e.g. broadcast from a loopback-only bind
                                if polls <= 3:
                                    print(f"ArtPollReply to {dest[0]} failed: {e}")
                    if polls <= 3:
                        print(f"ArtPoll from controller (answered: {not args.no_reply})")
                elif op == OP_DMX:
                    parsed = parse_dmx(data)
                    if parsed:
                        uni, seq, _phys, dmx = parsed
                        s = stats.setdefault(uni, {"frames": 0, "gaps": 0, "last_seq": None, "dmx": b"", "len": 0})
                        if seq and s["last_seq"] is not None and seq != (s["last_seq"] % 255) + 1:
                            s["gaps"] += 1
                        if seq:
                            s["last_seq"] = seq
                        s["frames"] += 1
                        s["dmx"] = dmx
                        s["len"] = len(dmx)
                        s["src_port"] = src_port
            now = time.time()
            if now - window_start >= 1.0:
                for uni, s in sorted(stats.items()):
                    if s["frames"] == 0:
                        continue
                    chans = s["dmx"][: args.channels]
                    print(f"{now - t0:6.1f}s univ={uni} {s['frames']:3d} fps gaps={s['gaps']} len={s['len']:3d}  {strands(chans, args.channels)}")
                    if writer:
                        writer.writerow([f"{now - t0:.1f}", uni, s["frames"], s["gaps"], s["len"], "", " ".join(map(str, chans))])
                    s["frames"] = 0
                    s["gaps"] = 0
                if not stats and polls == 0 and int(now - t0) % 5 == 0:
                    print(f"{now - t0:6.1f}s no Art-Net yet")
                window_start = now
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
        if writer:
            f.close()
    print(f"Done. ArtPolls seen: {polls}. Universes seen: {sorted(stats) or 'none'}")


if __name__ == "__main__":
    main()
