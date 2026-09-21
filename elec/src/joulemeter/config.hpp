#pragma once

#include <cstdint>

// adc pins, gpio 26-29
constexpr unsigned kVoltageAdc = 26;
constexpr unsigned kCurrentAdc = 27;

// rp2040 12-bit adc
constexpr float kAdcVref      = 3.3f;
constexpr float kAdcCountsMax = 4095.0f;

// bus voltage sensed through a divider: v_bus = v_adc * kVoltageDividerRatio
// design range 0-60 v, 60/3.3 maps full scale to vref
// placeholder, set from the actual resistor values
constexpr float kVoltageDividerRatio = 60.0f / 3.3f;

// current from the low-side shunt drop through an amp stage, 0-200 a unidirectional
// i = (v_adc - kCurrentZeroVolts) / kCurrentVoltsPerAmp
// volts_per_amp = shunt_ohms * amp_gain, placeholder until hardware is set
constexpr float kCurrentZeroVolts   = 0.0f;
constexpr float kCurrentVoltsPerAmp = 3.3f / 200.0f;

// sample period, also the integration dt for joules
constexpr std::uint32_t kSamplePeriodMs = 10;

// how often to report the running total over can
constexpr std::uint32_t kReportPeriodMs = 500;

// mcp251863 can-fd controller spi wiring, todo set from the pcb
constexpr unsigned kSpiSck  = 18;
constexpr unsigned kSpiMosi = 19;
constexpr unsigned kSpiMiso = 16;
constexpr unsigned kMcpCs   = 17;
constexpr unsigned kMcpStby = 15;
