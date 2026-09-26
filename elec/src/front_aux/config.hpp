#pragma once

// turn-signal indicator LEDs, left and right, driven from bus commands
constexpr unsigned kTurnLeft  = 0;
constexpr unsigned kTurnRight = 1;

// headlights, left and right, always driven together, from bus commands
constexpr unsigned kHeadlightL = 2;
constexpr unsigned kHeadlightR = 3;

// horn relay, on while a command holds it on
constexpr unsigned kHorn = 4;

// wheel-speed pulse inputs, front left and right, published as telemetry
constexpr unsigned kRpmLeft  = 8;
constexpr unsigned kRpmRight = 9;

// steering-angle sensor, ratiometric analog through a divider on adc channel 0, published as telemetry
// piher pst-360 swings 0.5-4.5v at 5v supply, divide to stay under the 3.3v adc max
constexpr unsigned kSteeringAdc = 26;

// mcp251863 can-fd controller spi wiring, per the FAux_V1.1 schematic
constexpr unsigned kSpiSck  = 18;
constexpr unsigned kSpiMosi = 19;
constexpr unsigned kSpiMiso = 16;
constexpr unsigned kMcpCs   = 17;
constexpr unsigned kMcpStby = 23;
constexpr unsigned kMcpInt  = 20;
