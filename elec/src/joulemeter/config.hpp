#pragma once

#include <cstdint>

#include "pico/stdlib.h"

// the pico's onboard LED, held on while the firmware runs
constexpr unsigned kStatusLed = PICO_DEFAULT_LED_PIN;

// adc pins, gpio 26-29
constexpr unsigned kVoltageAdc = 26;
constexpr unsigned kCurrentAdc = 27;

// rp2040 12-bit adc
constexpr float kAdcVref      = 3.3f;
constexpr float kAdcCountsMax = 4095.0f;

// bus voltage sensed through a divider: v_bus = v_adc * kVoltageDividerRatio
// r13 115k over r14 5k per the Joulemeter_V1.4 bom, full scale 79.2 v
constexpr float kVoltageDividerRatio = (115'000.0f + 5'000.0f) / 5'000.0f;

// current from the low-side shunt drop through an amp stage, 0-200 a unidirectional
// i = (v_adc - kCurrentZeroVolts) / kCurrentVoltsPerAmp
// volts_per_amp = shunt_ohms * amp_gain, placeholder until hardware is set
constexpr float kCurrentZeroVolts   = 0.0f;
constexpr float kCurrentVoltsPerAmp = 3.3f / 200.0f;

// how often to report period averages and the running total over can
constexpr std::uint32_t kReportPeriodMs = 500;

// mcp251863 can-fd controller spi wiring, per the Joulemeter_V1.4 schematic
constexpr unsigned kSpiSck  = 10;
constexpr unsigned kSpiMosi = 11;
constexpr unsigned kSpiMiso = 8;
constexpr unsigned kMcpCs   = 9;
constexpr unsigned kMcpStby = 12;
constexpr unsigned kMcpInt  = 13;
