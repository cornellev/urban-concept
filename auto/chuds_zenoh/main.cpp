#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "chuds/chuds.hpp"
#include "chuds/socketcan_transport.hpp"
#include "convert.hpp"
#include "zenoh.hxx"

namespace {

using Bus = chuds::Bus<chuds::SocketCanTransport>;

// zenoh rejects keys with a leading slash
constexpr std::string_view kControlKey       = "cev/commands/control";
constexpr std::string_view kLeftRpmKey       = "cev/sensors/wheels/left_rpm";
constexpr std::string_view kRightRpmKey      = "cev/sensors/wheels/right_rpm";
constexpr std::string_view kSteeringAngleKey = "cev/sensors/steering_angle";

// atomic, not sig_atomic_t, since the signal may land on a zenoh thread
std::atomic<bool> g_stop{false};

void on_signal(int /*sig*/) { g_stop = true; }

void publish(const zenoh::Session& session, std::string_view key, std::string payload) {
    zenoh::Session::PutOptions options;
    options.encoding = zenoh::Encoding::Predefined::application_json();
    zenoh::ZResult err{};
    session.put(zenoh::KeyExpr(key), zenoh::Bytes(std::move(payload)), std::move(options), &err);
    if (err != Z_OK) {
        std::fprintf(stderr, "put %.*s failed: %d\n", static_cast<int>(key.size()), key.data(),
                     err);
    }
}

void on_control(Bus& bus, const zenoh::Sample& sample) {
    const auto cmd = chuds_zenoh::parse_control(sample.get_payload().as_string());
    if (!cmd) {
        std::fprintf(stderr, "dropped a bad %.*s payload\n", static_cast<int>(kControlKey.size()),
                     kControlKey.data());
        return;
    }
    const auto st =
        bus.send(chuds::MsgClass::Command, chuds::merceves::kNodeId, chuds::MsgType::Update, *cmd);
    if (!st) {
        std::fprintf(stderr, "send drive command failed: %d\n", std::to_underlying(st.error()));
    }
}

void on_message(const zenoh::Session& session, const chuds::Message& m) {
    if (m.cls != chuds::MsgClass::Telemetry || m.subaddress != chuds::merceves::kNodeId) {
        return;
    }
    const auto t = chuds::body_as<chuds::merceves::Telemetry>(m);
    if (!t) {
        return;
    }
    auto out = chuds_zenoh::telemetry_payloads(*t);
    publish(session, kLeftRpmKey, std::move(out.left_rpm));
    publish(session, kRightRpmKey, std::move(out.right_rpm));
    publish(session, kSteeringAngleKey, std::move(out.steering_angle));
}

}  // namespace

int main(int argc, char** argv) {
    std::string_view ifname = "can0";
    if (argc == 3 && std::string_view{argv[1]} == "-i") {
        ifname = argv[2];
    } else if (argc != 1) {
        std::fprintf(stderr, "usage: %s [-i iface]\niface defaults to can0\n", argv[0]);
        return 2;
    }

    Bus bus;
    if (!bus.open(ifname)) {
        std::fprintf(stderr, "failed to open %.*s\n", static_cast<int>(ifname.size()),
                     ifname.data());
        return 1;
    }

    zenoh::ZResult err{};
    auto session = zenoh::Session::open(zenoh::Config::create_default(),
                                        zenoh::Session::SessionOptions::create_default(), &err);
    if (err != Z_OK) {
        std::fprintf(stderr, "failed to open a zenoh session: %d\n", err);
        return 1;
    }
    // zenoh calls this on its own threads, while the loop below reads the bus
    // safe because SocketCanTransport allows send and recv on different threads
    auto sub = session.declare_subscriber(
        zenoh::KeyExpr(kControlKey),
        [&bus](const zenoh::Sample& sample) { on_control(bus, sample); }, zenoh::closures::none);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    while (!g_stop) {
        if (!bus.wait(std::chrono::milliseconds{200})) {
            continue;
        }
        for (auto r = bus.recv(); r || r.error() != chuds::RxError::Empty; r = bus.recv()) {
            if (r) {
                on_message(session, *r);
            } else if (r.error() == chuds::RxError::Error) {
                std::fprintf(stderr, "recv error: %d\n", std::to_underlying(r.error()));
                return 1;
            } else if (chuds::is_fault(r.error())) {
                // overflow and bus-off recover, so report and keep reading
                std::fprintf(stderr, "recv fault: %d\n", std::to_underlying(r.error()));
                break;
            }
        }
    }
    return 0;
}
