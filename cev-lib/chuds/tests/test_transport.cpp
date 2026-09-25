#include <doctest/doctest.h>

#include <expected>
#include <optional>

#include "chuds/frame.hpp"
#include "chuds/transport.hpp"

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

struct NotATransport {};
}  // namespace

static_assert(Transport<Loopback>);
static_assert(!Transport<NotATransport>);

TEST_CASE("a transport moves a frame through send and recv") {
    Loopback t;
    CanFrame f{};
    f.id  = CanId::from_raw(0x123);
    f.len = 2;

    CHECK(t.send(f).has_value());
    const auto got = t.recv();
    REQUIRE(got.has_value());
    CHECK(got->id.raw() == 0x123);

    // consumed, so the next recv is Empty
    CHECK(t.recv() == std::unexpected(RxError::Empty));
}

TEST_CASE("is_fault separates bus problems from normal receive outcomes") {
    CHECK(is_fault(RxError::Overflow));
    CHECK(is_fault(RxError::BusOff));
    CHECK(is_fault(RxError::Error));
    CHECK_FALSE(is_fault(RxError::Empty));
    CHECK_FALSE(is_fault(RxError::ForeignFrame));
    CHECK_FALSE(is_fault(RxError::UnknownClass));
    CHECK_FALSE(is_fault(RxError::BodyOverrun));
}

TEST_CASE("is_fault treats a full tx queue as retryable") {
    CHECK(is_fault(TxError::BusOff));
    CHECK(is_fault(TxError::Error));
    CHECK_FALSE(is_fault(TxError::QueueFull));
}
