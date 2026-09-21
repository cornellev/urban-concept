#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <thread>

#include "chuds/io.hpp"
#include "chuds/message.hpp"
#include "chuds/socketcan_transport.hpp"

using namespace chuds;

namespace {

// a telemetry body: wheel speed in rpm
struct WheelSpeed {
    std::uint16_t rpm;
};
static_assert(sizeof(WheelSpeed) == 2);

constexpr std::uint8_t kWheelSensorId = 0x10;

int run_send(cev::SocketCanTransport& t) {
    std::uint16_t fake_rpm{12345};
    const auto msg =
        make_message(MsgClass::Telemetry, kWheelSensorId, MsgType::Update, WheelSpeed{fake_rpm});
    if (!msg) {
        std::fprintf(stderr, "error\n");
        return 1;
    }
    const auto st = send(t, *msg);
    std::printf("sent wheel speed %u. status: %d\n", static_cast<unsigned>(fake_rpm),
                static_cast<int>(st));
    return st == TxStatus::Queued ? 0 : 1;
}

int run_recv(cev::SocketCanTransport& t, std::string_view ifname) {
    std::printf("listening on %.*s\n", static_cast<int>(ifname.size()), ifname.data());
    for (int i = 0; i < 500; ++i) {
        const auto r = recv(t);
        if (r.status == RxStatus::BusOff) {
            std::fprintf(stderr, "bus off\n");
            return 1;
        }
        if (r.msg) {
            if (const auto w = body_as<WheelSpeed>(*r.msg)) {
                std::printf("recv wheel speed: %urpm from sensor 0x%02x\n",
                            static_cast<unsigned>(w->rpm),
                            static_cast<unsigned>(r.msg->subaddress));
            } else {
                std::printf("recv message type %d (not wheel speed)\n",
                            static_cast<int>(r.msg->type));
            }
            return 0;
        }
        // a foreign or malformed frame is not our fault; keep listening
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
