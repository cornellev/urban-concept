#pragma once

#include <array>

// TEST_LED on 12 and a second LED on 17
constexpr std::array<unsigned, 2> kStatusLeds{12, 17};

// mcp251863 can-fd controller spi wiring, per the SPI_2_CANFD_V1.2 schematic
constexpr unsigned kSpiSck  = 22;
constexpr unsigned kSpiMosi = 23;
constexpr unsigned kSpiMiso = 20;
constexpr unsigned kMcpCs   = 21;
constexpr unsigned kMcpStby = 24;
constexpr unsigned kMcpInt  = 25;
