#pragma once

#include <cstdint>

// turn-signal indicator LEDs, left and right, driven from bus commands
constexpr unsigned kTurnLeft  = 0;
constexpr unsigned kTurnRight = 1;

// wheel-speed pulse inputs, back left and right, published as telemetry
constexpr unsigned kRpmLeft  = 10;
constexpr unsigned kRpmRight = 11;

// windshield wiper servo, pwm output; bus command is on/off, the node runs the sweep
constexpr unsigned kWiperPwm = 13;

// wiper sweep arc and motion
constexpr int kWiperMinDeg  = -90;
constexpr int kWiperMaxDeg  = 90;
constexpr int kWiperParkDeg = -90;
// per tick, ~150 deg/s at the tick rate below
constexpr int kWiperStepDeg = 3;
// pause at each end of the sweep
constexpr std::uint32_t kWiperDwellMs = 100;
// one servo frame at 50 hz
constexpr std::uint32_t kWiperTickMs = 20;

// the car's running LED, held on
constexpr unsigned kRunningLed = 12;

// brake pedal sensor, 5v analog through a divider on adc channel 0, published as telemetry
constexpr unsigned kBrakeAdc = 26;

// mcp251863 can-fd controller spi wiring, per the BAux_V1.1 schematic
constexpr unsigned kSpiSck  = 2;
constexpr unsigned kSpiMosi = 3;
constexpr unsigned kSpiMiso = 4;
constexpr unsigned kMcpCs   = 5;
constexpr unsigned kMcpStby = 6;
constexpr unsigned kMcpInt  = 7;
