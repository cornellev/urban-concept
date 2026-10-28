#!/usr/bin/env python3
"""
Stream fake telemetry in the same format as the car's CSV logs. Run on the CAR Pi.
 
    python3 testpublisher.py --host <receiver-tailscale-IP> --rate 50          random values
    python3 testpublisher.py --host <receiver-tailscale-IP> --csv run1.csv     replay a real CSV
 
Random rows have the same 25 columns as the real logs, with values that drift
smoothly (speed, voltage, GPS position...) so they look like a car driving.
 
Each packet:   <seq>,<send time>,<one CSV row>
Header packet: H,0,<column names>      (sent at start and every 5 s, so a late receiver still gets it)
Works with testsubscriber.py unchanged.
"""
import argparse
import math
import random
import socket
import time
 
COLUMNS = [
    "seq", "global_ts",
    "power.ts", "power.current", "power.voltage",
    "steering.ts", "steering.brake_pressure", "steering.turn_angle",
    "rpm_front.ts", "rpm_front.rpm_left", "rpm_front.rpm_right",
    "rpm_back.ts", "rpm_back.rpm_left", "rpm_back.rpm_right",
    "gps.ts", "gps.lat", "gps.long", "gps.heading", "gps.speed",
    "motor.ts", "motor.rpm", "motor.throttle",
    "filtered.speed", "_t_publish_ns", "timestamp",
]
MAX_PAYLOAD = 1252  # largest UDP payload that fits Tailscale's 1280-byte MTU without splitting
 
p = argparse.ArgumentParser(description="Stream fake (or replayed) telemetry over UDP")
p.add_argument("--host", required=True, help="receiver's Tailscale IP (100.x.x.x)")
p.add_argument("--port", type=int, default=5006)
p.add_argument("--rate", type=float, default=50, help="rows per second (real logs are ~50)")
p.add_argument("--csv", help="replay this CSV instead of generating random rows")
p.add_argument("--duration", type=float, default=0, help="seconds to run (0 = until Ctrl+C)")
a = p.parse_args()
 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
dest = (a.host, a.port)
send_errors = 0
 
 
def send(payload):
    global send_errors
    data = payload.encode()
    if len(data) > MAX_PAYLOAD:
        print(f"warning: packet is {len(data)} bytes (> {MAX_PAYLOAD}); it will be split")
    try:
        sock.sendto(data, dest)
    except OSError as e:  # link hiccup: keep going so we can see recovery
        send_errors += 1
        if send_errors in (1, 10, 100) or send_errors % 1000 == 0:
            print(f"send error #{send_errors}: {e}")
 
 
class FakeCar:
    """Values that wander smoothly instead of jumping around."""
 
    def __init__(self):
        self.t0 = time.monotonic()
        self.speed = 8.0           # m/s
        self.voltage = 52.0        # V
        self.heading = random.uniform(0, 2 * math.pi)
        self.lat, self.lon = 39.7986, -86.2388
        self.turn = 0.36
        self.n = 0
 
    def row(self):
        self.n += 1
        now_ns = time.time_ns()
        ms = int((time.monotonic() - self.t0) * 1000)
        ts = 300_000_000 + ms   # sensor clocks, in the same style as the logs
 
        self.speed = min(12.0, max(0.0, self.speed + random.gauss(0, 0.05)))
        throttle = max(0.0, min(1.0, 0.4 + (9 - self.speed) * 0.1 + random.gauss(0, 0.03)))
        current = max(0.0, throttle * 4 + random.gauss(0, 0.15))
        self.voltage = max(46.0, self.voltage - current * 0.00002 + random.gauss(0, 0.01))
        self.turn = max(-1.0, min(1.0, self.turn + random.gauss(0, 0.004)))
        self.heading = (self.heading + self.turn * 0.002) % (2 * math.pi)
        step = self.speed / a.rate / 111_000  # metres -> degrees, roughly
        self.lat += step * math.cos(self.heading)
        self.lon += step * math.sin(self.heading) / math.cos(math.radians(self.lat))
        wheel = self.speed * 30  # wheel rpm from speed
        brake = max(0.0, random.gauss(0.02, 0.01))
 
        values = [
            self.n, ts,
            ts + 11, current, self.voltage,
            ts + 10, brake, self.turn,
            0, "nan", "nan",
            ts + 10, wheel + random.gauss(0, 4), wheel + random.gauss(0, 4),
            ts - ms % 1000, self.lat, self.lon, self.heading, self.speed,
            ts + 10, wheel * 5 * throttle, throttle,
            self.speed * 0.92, now_ns, now_ns + random.randint(500_000, 3_000_000),
        ]
        return ",".join(str(v) for v in values)
 
 
def rows():
    """Yield (header, row) pairs forever, or until the CSV ends."""
    if a.csv:
        with open(a.csv) as f:
            header = f.readline().strip()
            for line in f:
                if line.strip():
                    yield header, line.strip()
    else:
        car = FakeCar()
        header = ",".join(COLUMNS)
        while True:
            yield header, car.row()
 
 
source = f"replaying {a.csv}" if a.csv else "random telemetry"
print(f"streaming {source} to {dest[0]}:{dest[1]} at {a.rate:g} rows/s (Ctrl+C to stop)")
 
interval = 1.0 / a.rate
seq = 0
start = next_t = time.monotonic()
last_header = -1e9
try:
    for header, line in rows():
        now = time.monotonic()
        if a.duration and now - start >= a.duration:
            break
        if now - last_header >= 5.0:  # resend column names every 5 s
            send(f"H,0,{header}")
            last_header = now
 
        send(f"{seq},{time.time():.6f},{line}")
        seq += 1
        if seq % max(1, int(a.rate * 5)) == 0:
            print(f"sent {seq} rows")
 
        next_t += interval
        delay = next_t - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        elif delay < -1.0:  # fell far behind: don't burst to catch up
            next_t = time.monotonic()
except KeyboardInterrupt:
    pass
print(f"done: sent {seq} rows, {send_errors} send errors")
 