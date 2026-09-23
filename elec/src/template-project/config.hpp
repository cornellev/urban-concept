#pragma once

#include "pico/stdlib.h"

// the pico's onboard LED, blinked while the firmware runs
constexpr unsigned kStatusLed = PICO_DEFAULT_LED_PIN;

// mcp251863 can-fd controller spi wiring, todo set from the pcb
constexpr unsigned kSpiSck  = 18;
constexpr unsigned kSpiMosi = 19;
constexpr unsigned kSpiMiso = 16;
constexpr unsigned kMcpCs   = 17;
constexpr unsigned kMcpStby = 15;
