# CHUDS addressing

Status: draft, pending team ratification.

## Id layout

Every CHUDS frame uses an 11-bit standard CAN id. The top 3 bits are the message class, and the
low 8 bits are the subaddress. The lower id wins arbitration, so the class order is the priority
order.

| Class     | Value | Id range      | Subaddress means         |
| --------- | ----- | ------------- | ------------------------ |
| Emergency | `0`   | `0x000-0x0FF` | severity (0 most severe) |
| Command   | `1`   | `0x100-0x1FF` | recipient node id        |
| Telemetry | `2`   | `0x200-0x2FF` | source node id           |
| Reserved  | `3-7` | `0x300-0x7FF` | future classes           |

- Frames in the reserved range, and extended 29-bit frames, are rejected on receive.
- The bus is CHUDS-only. There is no magic byte, so any 11-bit CAN-FD frame with a plausible
  header decodes as CHUDS.

## Classes

- **Emergency:** broadcast, and every node acts on it. The subaddress is the severity, so `0x000`,
  the most severe STOP, wins every arbitration. A STOP body is `[sender id][detail]`. Only the VCU
  sends STOP, because CAN allows one transmitter per id. A node reports a fault to the VCU
  instead. This is the software stop. The hardwired e-stop works without any bus.
- **Command:** from the VCU to the nodes. The subaddress is the recipient node id. The low
  subaddresses are reserved for broadcasts. The one broadcast today is `BodyState` at `0x108`.
- **Telemetry:** each node publishes under its own subaddress, and any consumer filters for it.

## Assigning subaddresses

Within a class, the lower subaddress always wins the bus, so a chatty low-address node can starve
a higher one. Assign subaddresses by priority, not convenience. The allocation is part of the
protocol: changing a node's subaddress means updating every node that uses it.

## Payload

The CAN-FD data field is `[type][body_len][body...]`:

- `type`: `Update` (0), `Heartbeat` (1), `Stop` (2), `Action` (3), `Dummy` (4)
- `body_len`: the true body length. CAN-FD pads a frame up to the next valid size (0-8, 12, 16,
  20, 24, 32, 48, 64 bytes), and this byte tells body from padding.
- `body`: up to 62 bytes. Its layout is set by the (class, subaddress) pair and its exact size.
  Struct bodies follow [`serialization.md`](serialization.md).

## Examples

| Id      | Meaning                                                          |
| ------- | ---------------------------------------------------------------- |
| `0x000` | most severe STOP, every node halts                               |
| `0x108` | `BodyState` broadcast: turn signals, headlights, horn, wiper     |
| `0x140` | command to node `0x40`, such as an `Update` with a throttle body |
| `0x210` | telemetry from node `0x10` (front_aux)                           |

## Open questions

- Multicast groups (the proposal's Universal Firmware Id / Group Id / Domain Specific Id) as
  sub-ranges of the Command class.
- Whether a type is tied to a class, such as `Stop` only in the Emergency class.
- A sender id in payloads other than STOP.
- A send-rate limit per node.
- How long a node may go without a drive command before it stops the motor. For comparison, a
  node that hears no `BodyState` for 1 s switches its lights, horn, and wiper to safe states.
