#pragma once

#include <cstdint>
#include <cstdio>

#include "chuds/chuds.hpp"

namespace cev {

// prints the commands this node receives on usb serial, so a bench test can see them arrive
// call log() on every received message
struct RxLog {
    std::uint8_t node{};
    // body state bits last printed, without the blink phase, since the leader resends it constantly
    int last_body{-1};

    void log(const chuds::Message& m) {
        const bool body_state =
            m.cls == chuds::MsgClass::Command && m.subaddress == chuds::kBodyStateId;
        const bool to_me = m.cls == chuds::MsgClass::Command && m.subaddress == node;
        if (!body_state && !to_me && m.cls != chuds::MsgClass::Emergency) {
            return;
        }
        if (body_state) {
            if (const auto s = chuds::body_as<chuds::BodyState>(m)) {
                const int bits = s->bits & ~chuds::BodyState::kBlinkPhase;
                if (bits == last_body) {
                    return;
                }
                last_body = bits;
            }
        }
        std::printf("rx ");
        chuds::print_message(m);
    }
};

}  // namespace cev
