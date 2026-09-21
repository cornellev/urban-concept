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

// steering-angle sensor, analog on adc channel 0, published as telemetry
constexpr unsigned kSteeringAdc = 26;

// mcp251863 can-fd controller spi wiring, todo set from the pcb
constexpr unsigned kSpiSck  = 18;
constexpr unsigned kSpiMosi = 19;
constexpr unsigned kSpiMiso = 16;
constexpr unsigned kMcpCs   = 17;
constexpr unsigned kMcpStby = 15;
