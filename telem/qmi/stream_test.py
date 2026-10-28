#!/usr/bin/env python3
"""
Live UDP stream test for a cellular (wwan0) link.
 
"""
import argparse
import csv
import select
import socket
import statistics
import struct
import time
 
HDR = struct.Struct("!IId")  
MAGIC = 0x5EC0FFEE
 
 
def server(args):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", args.port))
    print(f"echo server listening on udp/{args.port}")
    seen = set()
    while True:
        data, addr = sock.recvfrom(65535)
        if len(data) < HDR.size or HDR.unpack_from(data)[0] != MAGIC:
            continue
        sock.sendto(data, addr)
        if addr not in seen:
            seen.add(addr)
            print(f"streaming from {addr[0]}:{addr[1]}")
 
 
def pct(sorted_vals, p):
    if not sorted_vals:
        return float("nan")
    k = min(len(sorted_vals) - 1, int(round(p / 100 * (len(sorted_vals) - 1))))
    return sorted_vals[k]
 
 
def client(args):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if args.iface:
        # Pin the stream to the cellular interface regardless of the default route.
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, args.iface.encode())
    sock.setblocking(False)
    dest = (socket.gethostbyname(args.host), args.port)
 
    size = max(args.size, HDR.size)
    pad = b"\x00" * (size - HDR.size)
    interval = 1.0 / args.rate
 
    f = writer = None
    if args.csv:
        f = open(args.csv, "w", newline="")
        writer = csv.writer(f)
        writer.writerow(["seq", "send_time_s", "rtt_ms"])
 
    start = time.monotonic()
    stop_sending = start + args.duration
    deadline = stop_sending + 3.0  # grace period for late echoes
    next_send = start
    next_report = start + 1.0
 
    seq = 0
    send_errors = 0
    got = set()
    dups = reordered = 0
    highest = -1
    rtts, win = [], []
    jitter, prev_rtt = 0.0, None
    last_rx, longest_gap = start, 0.0
    win_sent = win_rx = 0
 
    print(f"streaming to {dest[0]}:{dest[1]} via {args.iface or 'default route'} "
          f"at {args.rate:g} pkt/s x {size} B ({args.rate * size * 8 / 1000:.1f} kbit/s)")
    print(f"{'t(s)':>5} {'sent':>5} {'rx':>5} {'loss%':>6} {'p50ms':>7} {'maxms':>7} {'jitter':>7}")
 
    try:
        while True:
            now = time.monotonic()
            if now >= deadline:
                break
 
            # Send whatever is due. If we fell far behind (e.g. a stalled send), don't burst.
            if now - next_send > 1.0:
                next_send = now
            while next_send <= now < stop_sending:
                try:
                    sock.sendto(HDR.pack(MAGIC, seq, time.monotonic()) + pad, dest)
                except OSError as e:
                    send_errors += 1
                    if send_errors in (1, 10, 100) or send_errors % 1000 == 0:
                        print(f"  send error #{send_errors}: {e}")
                seq += 1
                win_sent += 1
                next_send += interval
 
            # Wait for echoes until the next send or report is due.
            wake = min(next_send if now < stop_sending else deadline, next_report)
            r, _, _ = select.select([sock], [], [], max(0.0, wake - time.monotonic()))
            while r:
                try:
                    data = sock.recv(65535)
                except (BlockingIOError, OSError):
                    break
                if len(data) < HDR.size:
                    continue
                magic, s, t_sent = HDR.unpack_from(data)
                if magic != MAGIC:
                    continue
                t_rx = time.monotonic()
                if s in got:
                    dups += 1
                    continue
                got.add(s)
                if s < highest:
                    reordered += 1
                highest = max(highest, s)
 
                rtt = (t_rx - t_sent) * 1000
                rtts.append(rtt)
                win.append(rtt)
                win_rx += 1
                if prev_rtt is not None:
                    jitter += (abs(rtt - prev_rtt) - jitter) / 16  # RFC 3550 estimator
                prev_rtt = rtt
                if t_rx <= stop_sending:
                    longest_gap = max(longest_gap, t_rx - last_rx)
                    last_rx = t_rx
                if writer:
                    writer.writerow([s, f"{t_sent - start:.4f}", f"{rtt:.2f}"])
 
            if time.monotonic() >= next_report:
                ws = sorted(win)
                loss = 100 * max(0, win_sent - win_rx) / win_sent if win_sent else 0.0
                flag = "  <-- NO ECHOES" if win_sent and not win_rx else ""
                print(f"{next_report - start:5.0f} {win_sent:5d} {win_rx:5d} {loss:6.1f} "
                      f"{pct(ws, 50):7.1f} {(ws[-1] if ws else float('nan')):7.1f} {jitter:7.1f}{flag}")
                win, win_sent, win_rx = [], 0, 0
                next_report += 1.0
    except KeyboardInterrupt:
        print("\ninterrupted")
 
    if f:
        f.close()
    end_sending = min(time.monotonic(), stop_sending)
    if last_rx < end_sending:
        longest_gap = max(longest_gap, end_sending - last_rx)
 
    sr = sorted(rtts)
    received = len(got)
    print("\n--- summary ---")
    print(f"sent {seq}, received {received}, lost {seq - received} "
          f"({100 * (seq - received) / seq if seq else 0:.2f}%), "
          f"dup {dups}, reordered {reordered}, send errors {send_errors}")
    if sr:
        print(f"rtt ms: min {sr[0]:.1f}  avg {statistics.mean(sr):.1f}  p50 {pct(sr, 50):.1f}  "
              f"p95 {pct(sr, 95):.1f}  p99 {pct(sr, 99):.1f}  max {sr[-1]:.1f}")
        print(f"jitter {jitter:.1f} ms, longest gap without echoes {longest_gap:.2f} s")
    else:
        print("no echoes received: check routing on wwan0, the server address, and its firewall (UDP)")
 
 
def main():
    p = argparse.ArgumentParser(description="UDP live-stream test over a cellular link")
    sub = p.add_subparsers(dest="mode", required=True)
    s = sub.add_parser("server")
    s.add_argument("--port", type=int, default=5005)
    c = sub.add_parser("client")
    c.add_argument("--host", required=True, help="echo server IP (public or Tailscale)")
    c.add_argument("--port", type=int, default=5005)
    c.add_argument("--iface", default="wwan0", help="interface to bind to ('' for default route)")
    c.add_argument("--rate", type=float, default=20, help="packets per second")
    c.add_argument("--size", type=int, default=256, help="bytes per packet")
    c.add_argument("--duration", type=float, default=60, help="seconds to stream")
    c.add_argument("--csv", help="write per-packet RTTs to this file")
    args = p.parse_args()
    server(args) if args.mode == "server" else client(args)
 
 
if __name__ == "__main__":
    main()
 