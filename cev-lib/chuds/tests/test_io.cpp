#include <doctest/doctest.h>

#include <array>
#include <optional>

#include "chuds/io.hpp"

using namespace chuds;

namespace {
// a loopback transport: send stores one frame, recv hands it back once
struct Loopback {
    std::optional<CanFrame> pending;
    TxStatus send(const CanFrame& f) {
        pending = f;
        return TxStatus::Queued;
    }
    RxResult recv() {
        if (!pending) {
            return RxResult{RxStatus::Empty, {}};
        }
        RxResult r{RxStatus::Received, *pending};
        pending.reset();
        return r;
    }
};
}  // namespace

TEST_CASE("send and recv move a Message without the caller touching a frame") {
    Loopback t;
    const std::array<std::uint8_t, 2> body{0x01, 0x1E};
    const auto m = make_message(MsgClass::Command, 0x40, MsgType::Update, body);
    REQUIRE(m.has_value());

    CHECK(send(t, *m) == TxStatus::Queued);

    const auto r = recv(t);
    REQUIRE(r.status == RxStatus::Received);
    REQUIRE(r.msg.has_value());
    CHECK(r.msg->cls == MsgClass::Command);
    CHECK(r.msg->subaddress == 0x40);
    CHECK(r.msg->type == MsgType::Update);
    REQUIRE(r.msg->body_view().size() == 2);
    CHECK(r.msg->body_view()[1] == 0x1E);
}

TEST_CASE("recv passes an empty bus through as Empty") {
    Loopback t;
    const auto r = recv(t);
    CHECK(r.status == RxStatus::Empty);
    CHECK_FALSE(r.msg.has_value());
}

TEST_CASE("recv reports a frame that arrives but does not decode as Malformed") {
    struct BadFrame {
        TxStatus send(const CanFrame&) { return TxStatus::Queued; }
        RxResult recv() {
            CanFrame f{};
            f.id  = CanId::from_raw(0x300);  // reserved class, decode rejects
            f.len = 2;
            return {RxStatus::Received, f};
        }
    } t;
    const auto r = recv(t);
    CHECK(r.status == RxStatus::Malformed);
    CHECK_FALSE(r.msg.has_value());
}

TEST_CASE("send reports Error when the message cannot be encoded") {
    Loopback t;
    Message m{};
    m.body_len = kMaxBody + 1;  // encode rejects
    CHECK(send(t, m) == TxStatus::Error);
}
