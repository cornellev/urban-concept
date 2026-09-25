#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>

#include "chuds/bus.hpp"
#include "chuds/catalog.hpp"
#include "chuds/frame.hpp"
#include "chuds/ids.hpp"
#include "chuds/message.hpp"
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

// a transport that hands back one reserved-class frame
struct BadFrame {
    TxResult send(const CanFrame& /*f*/) { return {}; }
    RxResult recv() {
        CanFrame f{};
        f.id  = CanId::from_raw(0x300);
        f.len = 2;
        return f;
    }
};

// a transport whose setup failed
struct NotReady {
    TxResult send(const CanFrame& /*f*/) { return std::unexpected(TxError::Error); }
    RxResult recv() { return std::unexpected(RxError::Error); }
    [[nodiscard]] bool ready() const { return false; }
};

// a transport whose bus is off
struct Dead {
    TxResult send(const CanFrame& /*f*/) { return std::unexpected(TxError::BusOff); }
    RxResult recv() { return std::unexpected(RxError::BusOff); }
};

// a transport that returns fixed results and counts sends
struct Scripted {
    TxResult tx;
    RxResult rx;
    int* sends;
    [[nodiscard]] TxResult send(const CanFrame& /*f*/) const {
        ++*sends;
        return tx;
    }
    [[nodiscard]] RxResult recv() const { return rx; }
};

// a transport that is bus-off on the first recv, then healthy
struct Recovers {
    int calls{};
    TxResult send(const CanFrame& /*f*/) { return {}; }
    RxResult recv() {
        return ++calls == 1 ? std::unexpected(RxError::BusOff) : std::unexpected(RxError::Empty);
    }
};
}  // namespace

static_assert(!std::is_copy_constructible_v<Bus<Loopback>>);
static_assert(!std::is_move_constructible_v<Bus<Loopback>>);

// true when a bus type exposes open(), which only SocketCAN-style transports should
template <class B>
concept CanOpen = requires(B& b) { b.open("can0"); };

static_assert(!CanOpen<Bus<Loopback>>);

// true when a bus type exposes ready(), which only transports that report setup should
template <class B>
concept CanReportReady = requires(const B& b) { b.ready(); };

static_assert(!CanReportReady<Bus<Loopback>>);

TEST_CASE("a bus sends and receives a message without the caller touching a frame") {
    Bus<Loopback> bus;
    const std::array<std::uint8_t, 2> body{0x01, 0x1E};
    const auto m = make_message(MsgClass::Command, 0x40, MsgType::Update, body);
    REQUIRE(m.has_value());

    CHECK(bus.send(*m).has_value());

    const auto r = bus.recv();
    REQUIRE(r.has_value());
    CHECK(r->cls == MsgClass::Command);
    CHECK(r->subaddress == 0x40);
    REQUIRE(r->body_view().size() == 2);
    CHECK(r->body_view()[1] == 0x1E);
    CHECK(bus.ok());
}

TEST_CASE("a bus builds a struct body message in one send call") {
    Bus<Loopback> bus;
    const BodyState state{BodyState::kHeadlights};
    CHECK(bus.send(MsgClass::Command, kBodyStateId, MsgType::Update, state).has_value());
    const auto r = bus.recv();
    REQUIRE(r.has_value());
    const auto back = body_as<BodyState>(*r);
    REQUIRE(back.has_value());
    CHECK(back->bits == BodyState::kHeadlights);
}

TEST_CASE("a quiet bus reports Empty and is not a fault") {
    Bus<Loopback> bus;
    CHECK(bus.recv() == std::unexpected(RxError::Empty));
    CHECK(bus.ok());
}

TEST_CASE("a frame that does not decode reports why and is not a fault") {
    Bus<BadFrame> bus;
    CHECK(bus.recv() == std::unexpected(RxError::UnknownClass));
    CHECK(bus.ok());
}

TEST_CASE("one bus fault latches ok to false") {
    Bus<Dead> bus;
    CHECK(bus.recv() == std::unexpected(RxError::BusOff));
    CHECK_FALSE(bus.ok());
}

TEST_CASE("a message that cannot be encoded is an Error and a fault") {
    Bus<Loopback> bus;
    Message m{};
    m.body_len = kMaxBody + 1;
    CHECK(bus.send(m) == std::unexpected(TxError::Error));
    CHECK_FALSE(bus.ok());
}

TEST_CASE("a bus reports a transport whose setup failed before any send or recv") {
    const Bus<NotReady> bus;
    CHECK_FALSE(bus.ready());
    CHECK(bus.ok());
}

TEST_CASE("every send fault latches ok, and a full queue does not") {
    for (const TxError e : {TxError::QueueFull, TxError::BusOff, TxError::Error}) {
        int sends{};
        Bus<Scripted> bus{Scripted{.tx = std::unexpected(e), .rx = {}, .sends = &sends}};
        CHECK(bus.send(Message{}) == std::unexpected(e));
        CHECK(bus.ok() == !is_fault(e));
    }
}

TEST_CASE("every receive fault latches ok") {
    for (const RxError e : {RxError::Overflow, RxError::BusOff, RxError::Error}) {
        int sends{};
        Bus<Scripted> bus{Scripted{.tx = {}, .rx = std::unexpected(e), .sends = &sends}};
        CHECK(bus.recv() == std::unexpected(e));
        CHECK_FALSE(bus.ok());
    }
}

TEST_CASE("ok stays false after the bus recovers") {
    Bus<Recovers> bus;
    CHECK(bus.recv() == std::unexpected(RxError::BusOff));
    CHECK(bus.recv() == std::unexpected(RxError::Empty));
    CHECK(bus.send(Message{}).has_value());
    CHECK_FALSE(bus.ok());
}

TEST_CASE("a struct send with an invalid class never reaches the transport") {
    int sends{};
    Bus<Scripted> bus{Scripted{.tx = {}, .rx = {}, .sends = &sends}};
    CHECK(bus.send(static_cast<MsgClass>(3), kBodyStateId, MsgType::Update, BodyState{}) ==
          std::unexpected(TxError::Error));
    CHECK_FALSE(bus.ok());
    CHECK(sends == 0);
}
