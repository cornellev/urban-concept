#pragma once

// turn-signal indicator LEDs, left and right, driven from bus commands
constexpr unsigned kTurnLeft  = 0;
constexpr unsigned kTurnRight = 1;

// wheel-speed pulse inputs, back left and right, published as telemetry
constexpr unsigned kRpmLeft  = 10;
constexpr unsigned kRpmRight = 11;

// windshield wiper servo, pwm output, driven from bus commands
constexpr unsigned kWiperPwm = 13;

// the car's running LED, held on
constexpr unsigned kRunningLed = 12;

// brake pedal sensor, 5v analog through a divider on adc channel 0, published as telemetry
constexpr unsigned kBrakeAdc = 26;

// mcp251863 can-fd controller spi wiring, todo set from the pcb
constexpr unsigned kSpiSck  = 18;
constexpr unsigned kSpiMosi = 19;
constexpr unsigned kSpiMiso = 16;
constexpr unsigned kMcpCs   = 17;
constexpr unsigned kMcpStby = 15;
