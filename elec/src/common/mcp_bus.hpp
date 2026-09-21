#pragma once

#include <cstdio>

#include "chuds/mcp_transport.hpp"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "mcp251863.h"
#include "pico/stdlib.h"

namespace cev {

struct McpPins {
    unsigned sck;
    unsigned mosi;
    unsigned miso;
    unsigned cs;
    unsigned stby;
};

// brings up spi0 and the mcp251863 on construction, then hands out the transport
class McpBus {
   public:
    // initialize spi0 and the mcp251863
    explicit McpBus(const McpPins& pins) : mcp_(spi0, pins.cs, pins.stby) {
        spi_init(spi0, MCP251863_BAUD_RATE);
        spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

        gpio_set_function(pins.sck, GPIO_FUNC_SPI);
        gpio_set_function(pins.mosi, GPIO_FUNC_SPI);
        gpio_set_function(pins.miso, GPIO_FUNC_SPI);

        if (!mcp_.init()) {
            std::printf("mcp251863 init failed\n");
        }
    }

    // prevent move and copy operations
    McpBus(const McpBus&)            = delete;
    McpBus& operator=(const McpBus&) = delete;
    McpBus(McpBus&&)                 = delete;
    McpBus& operator=(McpBus&&)      = delete;
    ~McpBus()                        = default;

    Mcp251863Transport& transport() { return tx_; }

   private:
    MCP251863 mcp_;
    Mcp251863Transport tx_{mcp_};
};

}  // namespace cev
