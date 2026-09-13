#include <doctest/doctest.h>

#include <optional>

#include "chuds/frame.hpp"
#include "chuds/transport.hpp"

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

struct NotATransport {};
}  // namespace

static_assert(Transport<Loopback>);
static_assert(!Transport<NotATransport>);

TEST_CASE("a transport moves a frame through the seam") {
    Loopback t;
    CanFrame f{};
    f.id  = 0x123;
    f.len = 2;

    CHECK(t.send(f) == TxStatus::Queued);
    const auto got = t.recv();
    REQUIRE(got.status == RxStatus::Received);
    CHECK(got.frame.id == 0x123);
    CHECK(t.recv().status == RxStatus::Empty);  // consumed
}
