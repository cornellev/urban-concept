#include <chrono>
#include <cstdio>
#include <string_view>
#include <utility>

#include "chuds/io.hpp"
#include "chuds/message.hpp"
#include "chuds/socketcan_transport.hpp"
#include "chuds/wire.hpp"
#include "chuds/wire_format.hpp"

namespace {

// send a body state with headlights on
int run_send(cev::SocketCanTransport& t) {
    const cev::BodyState state{cev::BodyState::kHeadlights};
    const auto msg = chuds::make_message(chuds::MsgClass::Command, cev::kBodyStateId,
                                         chuds::MsgType::Update, state);
    if (!msg) {
        std::fprintf(stderr, "error\n");
        return 1;
    }
    const auto st = chuds::send(t, *msg);
    if (!st) {
        std::fprintf(stderr, "send failed: %d\n", std::to_underlying(st.error()));
        return 1;
    }
    std::printf("sent body state 0x%02x\n", state.bits);
    return 0;
}

// print the first message that arrives
int run_recv(cev::SocketCanTransport& t, std::string_view ifname) {
    std::printf("listening on %.*s\n", static_cast<int>(ifname.size()), ifname.data());
    while (t.wait(std::chrono::seconds{5})) {
        const auto r = chuds::recv(t);
        if (r) {
            cev::print_message(*r);
            return 0;
        }
        // a dead bus or driver is fatal, a foreign or malformed frame is not
        if (r.error() == chuds::RxError::BusOff || r.error() == chuds::RxError::Error) {
            std::fprintf(stderr, "recv failed: %d\n", std::to_underlying(r.error()));
            return 1;
        }
    }
    std::fprintf(stderr, "timeout, nothing received\n");
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view mode   = argc > 1 ? argv[1] : "";
    const std::string_view ifname = argc > 2 ? argv[2] : "vcan0";

    if (mode != "send" && mode != "recv") {
        std::fprintf(stderr, "usage: %s send|recv [ifname]\n", argv[0]);
        return 2;
    }

    cev::SocketCanTransport t;
    if (!t.open(ifname)) {
        std::fprintf(stderr, "failed to open %.*s\n", static_cast<int>(ifname.size()),
                     ifname.data());
        return 1;
    }

    return mode == "send" ? run_send(t) : run_recv(t, ifname);
}
