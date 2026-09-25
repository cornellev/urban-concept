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

TEST_CASE("a transport moves a frame through the seam") {
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
