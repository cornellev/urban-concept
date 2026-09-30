#pragma once

#include <cstdint>
#include <cstdio>

#include "chuds/transport.hpp"
#include "common/interval.hpp"

namespace cev {

// prints the can controller's state once a second while it is not healthy
// call check() every loop pass, it reads the chip only when the second is up
struct CanHealth {
    Interval every_second{1000};

    template <class Bus>
    void check(Bus& bus) {
        if (!every_second.due()) {
            return;
        }
        const chuds::BusStatus s = bus.status();
        if (s.state == chuds::BusState::Active) {
            return;
        }
        std::printf("can %s, tx errors %u, rx errors %u\n",
                    s.state == chuds::BusState::Off ? "bus-off" : "error-passive",
                    static_cast<unsigned>(s.tx_errors), static_cast<unsigned>(s.rx_errors));
    }
};

}  // namespace cev
