#!/usr/bin/env python3
"""
Receive the telemetry stream from data_out.py and print it live. Run on the RECEIVER Pi.
 
"""
import argparse
import csv
import socket
 
p = argparse.ArgumentParser(description="Receive and print a UDP telemetry stream")
p.add_argument("--port", type=int, default=5006)
p.add_argument("--cols", help="comma-separated column names to show (default: all)")
p.add_argument("--every", type=int, default=1, help="print every Nth row (default: every row)")
a = p.parse_args()
 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", a.port))
 
header = None
wanted = [c.strip() for c in a.cols.split(",")] if a.cols else None
 
print(f"listening on udp/{a.port} (Ctrl+C to stop)")
try:
    while True:
        data, _ = sock.recvfrom(65535)
        try:
            kind, _sent, line = data.decode(errors="replace").split(",", 2)
        except ValueError:
            continue  # not one of our packets
        fields = next(csv.reader([line]))
 
        if kind == "H":  # header row: column names
            if header is None:
                header = fields
                if wanted:
                    missing = [c for c in wanted if c not in header]
                    if missing:
                        print(f"note: no such columns: {', '.join(missing)}")
            continue
 
        try:
            seq = int(kind)
        except ValueError:
            continue
        if seq % max(1, a.every):
            continue
 
        if header and len(header) == len(fields):
            pairs = list(zip(header, fields))
            if wanted:
                pairs = [(k, v) for k, v in pairs if k in wanted]
            print(f"#{seq:<7} " + "  ".join(f"{k}={v}" for k, v in pairs))
        else:
            print(f"#{seq:<7} " + ",".join(fields))  # header not received yet: raw values
except KeyboardInterrupt:
    pass
 