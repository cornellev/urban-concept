# Centralized Hardware Unification Dictation Scheme (CHUDS)

The car's CAN-FD protocol: a header-only C++23 library that builds unchanged for the RP2040 boards
and for the VCU.

## System

- **VCU (vehicle control unit, the leader):** the car's main computer, a Pi or a Jetson, one on
  the bus at a time. It reaches the bus through its own MCP251863 board, the kernel `mcp251xfd`
  driver, and SocketCAN. It sends `BodyState`, is the only node that sends STOP, and collects
  telemetry.
- **RP2040 boards (followers):** each talks to an MCP251863 over SPI. They publish telemetry and
  drive their outputs from `BodyState`.

## Bus settings

Every node must use the same settings, or frames fail with errors. They are defined once in
`frame.hpp` (`kNominalBitrate`, `kDataBitrate`, `kNominalSamplePoint`, `kDataSamplePoint`), and
`static_assert`s check the RP2040 driver's timing against them.

- **Two speeds.** A CAN-FD frame starts at the nominal rate, **500 kbit/s**, while nodes compete
  for the bus by id. Once one node has won, the data bytes switch to the data rate, **2 Mbit/s**.
- **80% sample point, in both phases.** A node reads each bit 80% of the way through it, after the
  signal has settled from the edge. The remaining 20% is slack that nodes use to stay in step with
  each other.

The RP2040 boards apply these in `Mcp251863Transport`. The VCU sets them when it brings up `can0`:

```sh
sudo ip link set can0 up type can bitrate 500000 sample-point 0.8 \
    dbitrate 2000000 dsample-point 0.8 fd on restart-ms 100
```

`bitrate`/`sample-point` set the nominal phase and `dbitrate`/`dsample-point` the data phase.
`fd on` enables CAN-FD frames. `restart-ms 100` restarts the controller 100 ms after it goes
bus-off, instead of leaving it off until someone runs the command again.

## Usage

Include `chuds/chuds.hpp` plus the transport for your platform, then do everything through a
`chuds::Bus`. Start from working code, which is compiled in CI and can't go stale:

- **RP2040 board:** [`template-project/main.cpp`](../../elec/src/template-project/main.cpp) is the
  starting point. [`front_aux/main.cpp`](../../elec/src/front_aux/main.cpp) is a full board that
  sends telemetry and applies `BodyState`.
- **VCU:** [`examples/chuds_demo.cpp`](examples/chuds_demo.cpp) sends a `BodyState` and prints
  what arrives over SocketCAN.

Things the code doesn't say:

- When `recv()` has no message, `rx.error()` says why. `Empty` just means nothing is waiting.
  `chuds::is_fault()` is true only for real problems such as bus-off.
- `body_as<T>` checks only the body size, so match the class and subaddress first.
- On the VCU, `bus.open("can0")` connects the transport and `bus.wait(timeout)` blocks until a
  frame arrives. Boards have neither: a board that takes commands polls the chip each loop pass.

## Files

| File                      | What it holds                                                 |
| ------------------------- | ------------------------------------------------------------- |
| `chuds.hpp`               | includes everything below except the two transports           |
| `bus.hpp`                 | `Bus<T>`: send, recv, and fault tracking                      |
| `catalog.hpp`             | the car's node ids and message bodies                         |
| `catalog_format.hpp`      | `format_message`, `print_message`                             |
| `message.hpp`             | `Message`, `encode`/`decode`, `make_message<T>`, `body_as<T>` |
| `transport.hpp`           | the `Transport` concept, `TxError`, `RxError`, `is_fault()`   |
| `frame.hpp`               | `CanFrame` and the bus settings                               |
| `ids.hpp`                 | `CanId`: class and subaddress                                 |
| `mcp_transport.hpp`       | `Mcp251863Transport`, RP2040 only                             |
| `socketcan_transport.hpp` | `SocketCanTransport`, Linux only                              |

Everything is in namespace `chuds`. The protocol is in [`docs/addressing.md`](docs/addressing.md),
and the rules for body structs are in [`docs/serialization.md`](docs/serialization.md).

## Build

Run these from `cev-lib/chuds/`, or prefix them with `cev-lib chuds` from the repo root
(`just cev-lib chuds test`):

```sh
just build     # configure + build
just test      # + run the doctest suite
just clean
```

The SocketCAN tests skip when `vcan0` is missing. Set `CHUDS_REQUIRE_CAN=1` to make that a
failure instead, as CI does.

The demo needs Linux and a virtual CAN interface:

```sh
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan && sudo ip link set vcan0 mtu 72 && sudo ip link set up vcan0
./build/examples/chuds_demo recv vcan0     # one terminal
./build/examples/chuds_demo send vcan0     # another
```

## Nodes

**Provisional until the team ratifies the id table.**

Each node owns one subaddress and publishes its telemetry at `0x2NN`. The VCU broadcasts the car's
body outputs as one `BodyState` bitmask on `0x108`, and each node drives only the outputs it has.
The VCU must resend it on change and well within every second, since boards fall back to safe
states after 1 s without one.

| Node       | Subaddress | Telemetry (`0x2NN`)             | Drives from `BodyState`    |
| ---------- | ---------- | ------------------------------- | -------------------------- |
| front_aux  | `0x10`     | rpm_left, rpm_right, steering   | turn L/R, headlights, horn |
| back_aux   | `0x20`     | rpm_left, rpm_right, brake      | turn L/R, wiper            |
| joulemeter | `0x30`     | voltage, current, power, energy | none                       |

Turn lamps light only while the `kBlinkPhase` bit is set. The VCU toggles it, so every node blinks
in phase. A node that hears no `BodyState` for 1 s falls back to safe states: horn off, wiper
parked, turn signals off, headlights held. A STOP applies the same safe states and latches until
reset.
