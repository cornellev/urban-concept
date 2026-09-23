#include <doctest/doctest.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "chuds/message.hpp"
#include "chuds/socketcan_transport.hpp"  // IWYU pragma: keep

using namespace chuds;

namespace {
// the interface the integration tests use; a vcan is enough
const char* test_if() {
    const char* e = std::getenv("CHUDS_TEST_CAN_IF");
    return (e != nullptr && *e != '\0') ? e : "vcan0";
}

// true when the test should skip for want of an interface
// CHUDS_REQUIRE_CAN turns the skip into a failure
bool skip_without_can(bool opened) {
    if (opened) {
        return false;
    }
    if (std::getenv("CHUDS_REQUIRE_CAN") != nullptr) {
        FAIL("no CAN-FD interface '" << test_if() << "' and CHUDS_REQUIRE_CAN is set");
    }
    MESSAGE("no CAN-FD interface '"
            << test_if()
            << "', skipping. set one up with: "
               "ip link add vcan0 type vcan && ip link set vcan0 mtu 72 && ip link set up vcan0");
    return true;
}

// recv is non-blocking, so poll briefly for a frame to arrive
RxResult recv_wait(cev::SocketCanTransport& t) {
    for (int i = 0; i < 200; ++i) {
        const auto r = t.recv();
        if (r.status != RxStatus::Empty) {
            return r;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return {RxStatus::Empty, {}};
}

// receive n frames and count how many were Malformed
int count_malformed(cev::SocketCanTransport& t, int n) {
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (recv_wait(t).status == RxStatus::Malformed) {
            ++count;
        }
    }
    return count;
}

// a raw CAN socket for injecting frames the driver would never send itself
int open_raw(bool fd) {
    int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        return -1;
    }
    if (fd) {
        int on{1};
        if (::setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &on, sizeof(on)) < 0) {
            ::close(s);
            return -1;
        }
    }
    ifreq ifr{};
    std::strncpy(ifr.ifr_name, test_if(), IFNAMSIZ - 1);
    if (::ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        ::close(s);
        return -1;
    }
    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(s);
        return -1;
    }
    return s;
}

// poll a raw socket briefly for a frame to arrive
ssize_t read_raw(int s, canfd_frame& cf) {
    for (int i = 0; i < 200; ++i) {
        const ssize_t n = ::recv(s, &cf, sizeof(cf), MSG_DONTWAIT);
        if (n >= 0) {
            return n;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return -1;
}
}  // namespace

static_assert(Transport<cev::SocketCanTransport>);

TEST_CASE("a default transport is closed and reports errors, not silence") {
    cev::SocketCanTransport t;
    CHECK_FALSE(t.is_open());
    CanFrame f{};
    f.id  = CanId::from_raw(0x123);
    f.len = 2;
    CHECK(t.send(f) == TxStatus::Error);
    CHECK(t.recv().status == RxStatus::Error);
}

TEST_CASE("opening a nonexistent interface fails") {
    cev::SocketCanTransport t;
    CHECK_FALSE(t.open("nosuchcan0"));
    CHECK_FALSE(t.is_open());
}

TEST_CASE("round-trips a chuds message over a CAN-FD interface") {
    cev::SocketCanTransport tx;
    cev::SocketCanTransport rx;
    if (skip_without_can(rx.open(test_if()) && tx.open(test_if()))) {
        return;
    }
    const std::array<std::uint8_t, 3> body{0x01, 0x39, 0x05};
    const auto msg = make_message(MsgClass::Telemetry, 0x10, MsgType::Update, body);
    REQUIRE(msg.has_value());
    const auto frame = encode(*msg);
    REQUIRE(frame.has_value());
    REQUIRE(tx.send(*frame) == TxStatus::Queued);

    const auto r = recv_wait(rx);
    REQUIRE(r.status == RxStatus::Received);
    const auto out = decode(r.frame);
    REQUIRE(out.has_value());
    CHECK(out->cls == MsgClass::Telemetry);
    CHECK(out->subaddress == 0x10);
    CHECK(out->type == MsgType::Update);
    REQUIRE(out->body_view().size() == 3);
    CHECK(out->body_view()[2] == 0x05);
}

TEST_CASE("recv reports empty on a quiet bus") {
    cev::SocketCanTransport rx;
    if (skip_without_can(rx.open(test_if()))) {
        return;
    }
    CHECK(rx.recv().status == RxStatus::Empty);
}

TEST_CASE("send rejects a malformed frame before it reaches the wire") {
    cev::SocketCanTransport tx;
    if (skip_without_can(tx.open(test_if()))) {
        return;
    }
    CanFrame bad_id{};
    bad_id.id  = CanId::from_raw(0x800);  // past the 11-bit standard range
    bad_id.len = 2;
    CHECK(tx.send(bad_id) == TxStatus::Error);

    CanFrame bad_len{};
    bad_len.id  = CanId::from_raw(0x100);
    bad_len.len = 65;  // past the FD payload
    CHECK(tx.send(bad_len) == TxStatus::Error);

    bad_len.len = 9;  // not a CAN-FD size
    CHECK(tx.send(bad_len) == TxStatus::Error);
}

TEST_CASE("recv rejects foreign frames instead of forging a standard id") {
    cev::SocketCanTransport rx;
    if (skip_without_can(rx.open(test_if()))) {
        return;
    }
    const int classic = open_raw(false);
    REQUIRE(classic >= 0);
    can_frame cf{};
    cf.can_id  = 0x123;
    cf.can_dlc = 2;
    REQUIRE(::write(classic, &cf, sizeof(cf)) == static_cast<ssize_t>(sizeof(cf)));

    const int ext = open_raw(true);
    REQUIRE(ext >= 0);
    canfd_frame ef{};
    ef.can_id = 0x800 | CAN_EFF_FLAG;  // would alias to 0x000 (STOP) if masked
    ef.len    = 8;
    REQUIRE(::write(ext, &ef, sizeof(ef)) == static_cast<ssize_t>(sizeof(ef)));

    // every foreign frame is Malformed, never a decodable Received
    CHECK(count_malformed(rx, 2) == 2);
    ::close(classic);
    ::close(ext);
}

TEST_CASE("recv rejects remote frames and ids past the standard range") {
    cev::SocketCanTransport rx;
    if (skip_without_can(rx.open(test_if()))) {
        return;
    }
    const int raw = open_raw(true);
    REQUIRE(raw >= 0);
    canfd_frame rtr{};
    rtr.can_id = 0x123 | CAN_RTR_FLAG;
    rtr.len    = 8;
    REQUIRE(::write(raw, &rtr, sizeof(rtr)) == static_cast<ssize_t>(sizeof(rtr)));

    canfd_frame wide{};
    wide.can_id = 0x800;  // no EFF flag, but past the 11-bit range
    wide.len    = 8;
    REQUIRE(::write(raw, &wide, sizeof(wide)) == static_cast<ssize_t>(sizeof(wide)));

    CHECK(count_malformed(rx, 2) == 2);
    ::close(raw);
}

TEST_CASE("send puts the exact id, BRS flag, length, and bytes on the wire") {
    cev::SocketCanTransport tx;
    if (skip_without_can(tx.open(test_if()))) {
        return;
    }
    const int raw = open_raw(true);
    REQUIRE(raw >= 0);

    CanFrame f{};
    f.id  = CanId::from_raw(0x123);
    f.len = 12;
    for (std::uint8_t i = 0; i < f.len; ++i) {
        f.data[i] = static_cast<std::uint8_t>(0xA0 + i);
    }
    REQUIRE(tx.send(f) == TxStatus::Queued);

    canfd_frame cf{};
    REQUIRE(read_raw(raw, cf) == static_cast<ssize_t>(sizeof(cf)));
    CHECK(cf.can_id == 0x123);
    CHECK((cf.flags & CANFD_BRS) != 0);
    CHECK(cf.len == 12);
    for (std::uint8_t i = 0; i < f.len; ++i) {
        CHECK(cf.data[i] == f.data[i]);
    }
    ::close(raw);
}

TEST_CASE("a bus-off error frame holds BusOff until a restart") {
    cev::SocketCanTransport t;
    if (skip_without_can(t.open(test_if()))) {
        return;
    }
    const int raw = open_raw(false);
    REQUIRE(raw >= 0);
    can_frame err{};
    err.can_id  = CAN_ERR_FLAG | CAN_ERR_BUSOFF;
    err.can_dlc = CAN_ERR_DLC;
    REQUIRE(::write(raw, &err, sizeof(err)) == static_cast<ssize_t>(sizeof(err)));

    REQUIRE(recv_wait(t).status == RxStatus::BusOff);
    // a quiet bus after bus-off is still BusOff, not Empty
    CHECK(t.recv().status == RxStatus::BusOff);

    CanFrame f{};
    f.id  = CanId::from_raw(0x123);
    f.len = 2;
    CHECK(t.send(f) == TxStatus::BusOff);

    err.can_id = CAN_ERR_FLAG | CAN_ERR_RESTARTED;
    REQUIRE(::write(raw, &err, sizeof(err)) == static_cast<ssize_t>(sizeof(err)));

    auto status = RxStatus::BusOff;
    for (int i = 0; i < 200 && status == RxStatus::BusOff; ++i) {
        status = t.recv().status;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(status == RxStatus::Empty);
    ::close(raw);
}
