#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
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
constexpr std::uint16_t kRpm          = 12345;  // fake payload

int run_send(cev::SocketCanTransport& t) {
    std::uint16_t fake_rpm{12345};
    const auto msg =
        make_message(MsgClass::Telemetry, kWheelSensorId, MsgType::Update, WheelSpeed{fake_rpm});
    if (!msg) {
        std::cerr << "error\n";
        return 1;
    }
    const auto st = send(t, *msg);
    std::cout << "sent wheel speed " << fake_rpm << ". status: " << static_cast<int>(st) << "\n";
    return st == TxStatus::Queued ? 0 : 1;
}

int run_recv(cev::SocketCanTransport& t, std::string_view ifname) {
    std::cout << "listening on " << ifname << "\n";
    for (int i = 0; i < 500; ++i) {
        const auto r = recv(t);
        if (r.status == RxStatus::BusOff) {
            std::cerr << "bus off\n";
            return 1;
        }
        if (r.msg) {
            if (const auto w = body_as<WheelSpeed>(*r.msg)) {
                std::cout << "recv wheel speed: " << w->rpm << "rpm from sensor" << "0x" << std::hex
                          << std::setw(2) << std::setfill('0') << +r.msg->subaddress << "\n";

            } else {
                std::cout << "recv message type " << static_cast<int>(r.msg->type)
                          << " (not wheel speed)\n";
            }
            return 0;
        }
        // a foreign or malformed frame is not our fault; keep listening
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cerr << "timeout, nothing received";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view mode   = argc > 1 ? argv[1] : "";
    const std::string_view ifname = argc > 2 ? argv[2] : "vcan0";

    if (mode != "send" && mode != "recv") {
        std::cerr << "usage: " << argv[0] << " send|recv [ifname]\n";
        return 2;
    }

    cev::SocketCanTransport t;
    if (!t.open(ifname)) {
        std::cerr << "failed to open " << ifname << "\n";
        return 1;
    }

    return mode == "send" ? run_send(t) : run_recv(t, ifname);
}
