"""
Bridge a spi2canfd dev board's USB serial to a SocketCAN interface, usually vcan0

Set up vcan0 once per boot:
    sudo modprobe vcan
    sudo ip link add vcan0 type vcan
    sudo ip link set vcan0 mtu 72 up

Run:
    python3 serial_bridge.py /dev/ttyACM0 vcan0
"""

import os
import re
import select
import socket
import struct
import sys
import tty

# struct canfd_frame: id, len, flags, two reserved bytes, 64 data bytes
CANFD_FMT = "=IBBBB64s"
CANFD_SIZE = struct.calcsize(CANFD_FMT)
CANFD_BRS = 0x01
CANFD_FDF = 0x04

# the firmware's frame lines, cansend fd syntax: "123##1DEADBEEF"
FRAME_LINE = re.compile(r"([0-9A-F]{3})##[0-9A-F]((?:[0-9A-F]{2})*)", re.IGNORECASE)


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} <tty> <can interface>")
    tty_path, ifname = sys.argv[1], sys.argv[2]

    serial = os.open(tty_path, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(serial)

    sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FD_FRAMES, 1)
    sock.bind((ifname,))

    pending = b""
    while True:
        ready, _, _ = select.select([serial, sock], [], [])

        if serial in ready:
            chunk = os.read(serial, 4096)
            if not chunk:
                sys.exit("serial closed")
            *lines, pending = (pending + chunk).split(b"\n")
            for raw in lines:
                line = raw.strip().decode(errors="replace")
                m = FRAME_LINE.fullmatch(line)
                if not m:
                    # status text from the board
                    print(line, file=sys.stderr)
                    continue
                data = bytes.fromhex(m.group(2))
                sock.send(struct.pack(CANFD_FMT, int(m.group(1), 16), len(data),
                                      CANFD_BRS | CANFD_FDF, 0, 0, data))

        if sock in ready:
            raw = sock.recv(CANFD_SIZE)
            # chuds is CAN-FD only, so classic frames are dropped
            if len(raw) != CANFD_SIZE:
                continue
            can_id, length, _, _, _, data = struct.unpack(CANFD_FMT, raw)
            os.write(serial, f"{can_id:03X}##1{data[:length].hex().upper()}\n".encode())


if __name__ == "__main__":
    main()
