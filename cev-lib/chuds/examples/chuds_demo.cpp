#include <chrono>
#include <cstdio>
#include <string_view>
#include <utility>

#include "chuds/chuds.hpp"
#include "chuds/socketcan_transport.hpp"

namespace {

// send a body state with headlights on
int run_send(chuds::Bus<chuds::SocketCanTransport>& bus) {
    const chuds::BodyState state{chuds::BodyState::kHeadlights};
    const auto st =
        bus.send(chuds::MsgClass::Command, chuds::kBodyStateId, chuds::MsgType::Update, state);
    if (!st) {
        std::fprintf(stderr, "send failed: %d\n", std::to_underlying(st.error()));
        return 1;
    }
    std::printf("sent body state 0x%02x\n", state.bits);
    return 0;
}

// print the first message that arrives
int run_recv(chuds::Bus<chuds::SocketCanTransport>& bus, std::string_view ifname) {
    std::printf("listening on %.*s\n", static_cast<int>(ifname.size()), ifname.data());
    while (bus.wait(std::chrono::seconds{5})) {
        const auto r = bus.recv();
        if (r) {
            chuds::print_message(*r);
            return 0;
        }
        if (chuds::is_fault(r.error())) {
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

    chuds::Bus<chuds::SocketCanTransport> bus;
    if (!bus.open(ifname)) {
        std::fprintf(stderr, "failed to open %.*s\n", static_cast<int>(ifname.size()),
                     ifname.data());
        return 1;
    }

    return mode == "send" ? run_send(bus) : run_recv(bus, ifname);
}
