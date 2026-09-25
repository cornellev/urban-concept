#include <doctest/doctest.h>

#include <array>
#include <expected>
#include <optional>

#include "chuds/io.hpp"

using namespace chuds;

namespace {
// a loopback transport: send stores one frame, recv hands it back once
struct Loopback {
    std::optional<CanFrame> pending;
    TxResult send(const CanFrame& f) {
        pending = f;
        return {};
    }
    RxResult recv() {
        if (!pending) {
            return std::unexpected(RxError::Empty);
        }
        const CanFrame f = *pending;
        pending.reset();
        return f;
    }
};
}  // namespace

TEST_CASE("send and recv move a Message without the caller touching a frame") {
    Loopback t;
    const std::array<std::uint8_t, 2> body{0x01, 0x1E};
    const auto m = make_message(MsgClass::Command, 0x40, MsgType::Update, body);
    REQUIRE(m.has_value());

    CHECK(send(t, *m).has_value());

    const auto r = recv(t);
    REQUIRE(r.has_value());
    const Message& got = *r;
    CHECK(got.cls == MsgClass::Command);
    CHECK(got.subaddress == 0x40);
    CHECK(got.type == MsgType::Update);
    REQUIRE(got.body_view().size() == 2);
    CHECK(got.body_view()[1] == 0x1E);
}

TEST_CASE("recv passes an empty bus through as Empty") {
    Loopback t;
    CHECK(recv(t) == std::unexpected(RxError::Empty));
}

TEST_CASE("recv reports why a frame that arrives does not decode") {
    struct BadFrame {
        TxResult send(const CanFrame& /*f*/) { return {}; }
        RxResult recv() {
            CanFrame f{};
            f.id  = CanId::from_raw(0x300);  // reserved class, decode rejects
            f.len = 2;
            return f;
        }
    } t;
    const auto r = recv(t);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == RxError::UnknownClass);
}

TEST_CASE("recv passes a transport fault through unchanged") {
    struct Dead {
        TxResult send(const CanFrame& /*f*/) { return std::unexpected(TxError::BusOff); }
        RxResult recv() { return std::unexpected(RxError::BusOff); }
    } t;
    const auto r = recv(t);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == RxError::BusOff);
}

TEST_CASE("send reports Error when the message cannot be encoded") {
    Loopback t;
    Message m{};
    m.body_len   = kMaxBody + 1;  // encode rejects
    const auto r = send(t, m);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == TxError::Error);
}
