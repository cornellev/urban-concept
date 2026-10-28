#!/usr/bin/env python3
"""
Replay a telemetry CSV over UDP as if it were a live stream: one CSV line per packet.

"""
import argparse
import socket
import time
 
MAX_PAYLOAD = 1252  # largest UDP payload that fits Tailscale's 1280-byte MTU without splitting
 
p = argparse.ArgumentParser(description="Replay a CSV file as a live UDP telemetry stream")
p.add_argument("csv", help="path to the telemetry CSV file")
p.add_argument("--host", required=True, help="receiver's Tailscale IP (100.x.x.x)")
p.add_argument("--port", type=int, default=5006)
p.add_argument("--rate", type=float, default=10, help="lines (packets) per second")
p.add_argument("--loop", action="store_true", help="start over when the file ends")
p.add_argument("--no-header", action="store_true", help="the CSV has no header row")
a = p.parse_args()
 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
dest = (a.host, a.port)
interval = 1.0 / a.rate
send_errors = 0
 
 
def send(payload):
    global send_errors
    data = payload.encode()
    if len(data) > MAX_PAYLOAD:
        print(f"warning: packet is {len(data)} bytes (> {MAX_PAYLOAD}); it will be split and is more likely to be lost")
    try:
        sock.sendto(data, dest)
    except OSError as e:
        # Link hiccup (e.g. cellular dropped). Keep going so we can see recovery.
        send_errors += 1
        if send_errors in (1, 10, 100) or send_errors % 1000 == 0:
            print(f"send error #{send_errors}: {e}")
 
 
print(f"replaying {a.csv} to {dest[0]}:{dest[1]} at {a.rate:g} lines/s (Ctrl+C to stop)")
seq = 0
next_t = time.monotonic()
try:
    while True:
        with open(a.csv) as f:
            if not a.no_header:
                header = f.readline().strip()
                for _ in range(3):  # send it a few times in case one is lost
                    send(f"H,0,{header}")
            for line in f:
                line = line.strip()
                if not line:
                    continue
                send(f"{seq},{time.time():.6f},{line}")
                seq += 1
                if seq % max(1, int(a.rate * 5)) == 0:
                    print(f"sent {seq} lines")
                next_t += interval
                delay = next_t - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                elif delay < -1.0:  # fell far behind; don't burst to catch up
                    next_t = time.monotonic()
        if not a.loop:
            break
except KeyboardInterrupt:
    pass
print(f"done: sent {seq} lines, {send_errors} send errors")
 