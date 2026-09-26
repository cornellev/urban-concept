# cev-lib

Shared CEV libraries used across firmware and software

`CHUDS`, the RP2040/Pi/Jetson CAN-FD protocol, lives under `chuds/` (header-only, C++23).
There are two transports:

- `chuds_socketcan` for Linux SocketCAN
- `chuds_mcp` for the RP2040, which links the MCP251863 driver.

`RP2040_MCP251863_Driver/` is the RP2040 driver for the MCP251863 CAN-FD controller and transceiver
