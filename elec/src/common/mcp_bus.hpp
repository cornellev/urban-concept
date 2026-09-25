#pragma once

#include <cstdint>
#include <cstdio>
#include <expected>

#include "chuds/io.hpp"
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
    unsigned nint;
};

// brings up the spi block wired to the pins and the mcp251863 on construction
class McpBus {
   public:
    // initialize spi and the mcp251863
    // rp2040 spi pins alternate between spi0 and spi1 in banks of 8
    explicit McpBus(const McpPins& pins)
        : spi_(spi_get_instance(pins.sck / 8 % 2)),
          nint_(pins.nint),
          mcp_(spi_, pins.cs, pins.stby) {
        spi_init(spi_, MCP251863_BAUD_RATE);
        spi_set_format(spi_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_set_function(pins.sck, GPIO_FUNC_SPI);
        gpio_set_function(pins.mosi, GPIO_FUNC_SPI);
        gpio_set_function(pins.miso, GPIO_FUNC_SPI);

        // nINT is push-pull once the mcp is up, the pull-up holds it idle until then
        gpio_init(nint_);
        gpio_set_dir(nint_, GPIO_IN);
        gpio_pull_up(nint_);

        ok_ = mcp_.init();
        if (!ok_) {
            std::printf("mcp251863 init failed\n");
        }
    }

    // prevent move and copy operations
    McpBus(const McpBus&)            = delete;
    McpBus& operator=(const McpBus&) = delete;
    McpBus(McpBus&&)                 = delete;
    McpBus& operator=(McpBus&&)      = delete;
    ~McpBus()                        = default;

    chuds::Mcp251863Transport& transport() { return tx_; }

    // whether the controller initialized and no bus fault has been seen since
    [[nodiscard]] bool ok() const { return ok_; }

    // receive the next message, printing the first rx fault
    // nINT stays high while no frame or overflow is pending, so that skips the spi reads
    [[nodiscard]] chuds::RecvResult recv() {
        if (gpio_get(nint_)) {
            return std::unexpected(chuds::RxError::Empty);
        }
        auto rx = chuds::recv(tx_);
        if (ok_ && !rx &&
            (rx.error() == chuds::RxError::BusOff || rx.error() == chuds::RxError::Overflow)) {
            std::printf("can rx fault\n");
            ok_ = false;
        }
        return rx;
    }

    // send a telemetry body from a node, printing the first tx fault
    template <class Body>
    void publish(std::uint8_t node, const Body& body) {
        const auto m =
            chuds::make_message(chuds::MsgClass::Telemetry, node, chuds::MsgType::Update, body);
        const chuds::TxResult st =
            m ? chuds::send(tx_, *m) : chuds::TxResult{std::unexpected(chuds::TxError::Error)};
        if (ok_ && !st &&
            (st.error() == chuds::TxError::BusOff || st.error() == chuds::TxError::Error)) {
            std::printf("can tx fault\n");
            ok_ = false;
        }
    }

   private:
    spi_inst_t* spi_;
    unsigned nint_;
    MCP251863 mcp_;
    // latched so a persistent fault prints once, not every loop
    bool ok_{};
    chuds::Mcp251863Transport tx_{mcp_};
};

}  // namespace cev
