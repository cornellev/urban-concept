#include <cstdint>
#include <cstdio>

#include "chuds/chuds.hpp"
#include "chuds/mcp_transport.hpp"
#include "common/can_health.hpp"
#include "common/interval.hpp"
#include "config.hpp"
#include "pico/stdlib.h"

constexpr std::uint32_t kBlinkHalfPeriodMs = 500;
// frames handled per loop pass, the mcp rx fifo depth, so a flooded bus cannot starve the timers
constexpr int kMaxRxPerPass = 8;

// template node: blinks the LED and drains the bus
int main() {
    stdio_init_all();

    gpio_init(kStatusLed);
    gpio_set_dir(kStatusLed, GPIO_OUT);

    chuds::Bus<chuds::Mcp251863Transport> bus{chuds::Mcp251863Transport{{
        .sck  = kSpiSck,
        .mosi = kSpiMosi,
        .miso = kSpiMiso,
        .cs   = kMcpCs,
        .stby = kMcpStby,
        .nint = kMcpInt,
    }}};

    if (!bus.ready()) {
        std::printf("can init failed\n");
    }

    bool led_on{};
    cev::Interval blink{kBlinkHalfPeriodMs};
    cev::CanHealth can_health;

    while (true) {
        can_health.check(bus);
        for (int i = 0; i < kMaxRxPerPass; ++i) {
            const auto rx = bus.recv();
            if (!rx) {
                break;
            }
            // handle messages addressed to this node here
        }

        if (blink.due()) {
            led_on = !led_on;
            gpio_put(kStatusLed, led_on);
        }
    }
}
