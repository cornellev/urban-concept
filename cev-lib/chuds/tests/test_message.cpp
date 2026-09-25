#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include "chuds/catalog.hpp"
#include "chuds/message.hpp"

using namespace chuds;

namespace {
struct WheelSpeed {
    std::uint16_t rpm;
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(WheelSpeed) == 2);
}  // namespace

// encode and decode run at compile time (constexpr)
namespace {
constexpr Message kSample{
    .cls        = MsgClass::Command,
    .subaddress = 0x40,
    .type       = MsgType::Update,
    .body       = {0x01, 0x1E},
    .body_len   = 2,
};
constexpr auto kFrame = encode(kSample);
static_assert(kFrame.has_value());
static_assert(kFrame->id.raw() == 0x140);
static_assert(kFrame->len == 4);
constexpr auto kBack = decode(*kFrame);
static_assert(kBack.has_value());
static_assert(kBack->subaddress == 0x40);
static_assert(kBack->body_len == 2);
}  // namespace

TEST_CASE("encode lays out id, type, length, then body") {
    const auto f = encode(kSample);
    REQUIRE(f.has_value());
    CHECK(f->id.raw() == 0x140);
    CHECK(f->len == 4);
    CHECK(f->data[0] == std::to_underlying(MsgType::Update));
    CHECK(f->data[1] == 2);
    CHECK(f->data[2] == 0x01);
    CHECK(f->data[3] == 0x1E);
}

TEST_CASE("decode round-trips encode") {
    const Message m{
        .cls        = MsgClass::Telemetry,
        .subaddress = 0x10,
        .type       = MsgType::Action,
        .body       = {0xAB},
        .body_len   = 1,
    };
    const auto f = encode(m);
    REQUIRE(f.has_value());
    const auto out = decode(*f);
    REQUIRE(out.has_value());
    CHECK(out->cls == MsgClass::Telemetry);
    CHECK(out->subaddress == 0x10);
    CHECK(out->type == MsgType::Action);
    REQUIRE(out->body_len == 1);
    CHECK(out->body[0] == 0xAB);
}

TEST_CASE("decode recovers the true body length past CAN-FD padding") {
    CanFrame f{};
    f.id      = CanId(MsgClass::Telemetry, 0x10);
    f.data[0] = std::to_underlying(MsgType::Update);
    f.data[1] = 3;  // true body length
    f.data[2] = 0xAA;
    f.data[3] = 0xBB;
    f.data[4] = 0xCC;
    f.data[5] = 0xFF;  // padding
    f.data[6] = 0xFF;
    f.data[7] = 0xFF;
    f.len     = 8;

    const auto out = decode(f);
    REQUIRE(out.has_value());
    REQUIRE(out->body_len == 3);
    CHECK(out->body[0] == 0xAA);
    CHECK(out->body[2] == 0xCC);
}

TEST_CASE("make_message builds from a span and body_view reads it back") {
    const std::array<std::uint8_t, 3> data{0x11, 0x22, 0x33};
    const auto m = make_message(MsgClass::Telemetry, 0x05, MsgType::Update, data);
    REQUIRE(m.has_value());
    const auto view = m->body_view();
    REQUIRE(view.size() == 3);
    CHECK(view[0] == 0x11);
    CHECK(view[2] == 0x33);
}

TEST_CASE("encode rounds a non-DLC length up to a valid CAN-FD size") {
    // 9 body bytes -> payload 11 -> not a valid DLC, rounds to 12
    const std::array<std::uint8_t, 9> data{};
    const auto m = make_message(MsgClass::Telemetry, 0x01, MsgType::Update, data);
    REQUIRE(m.has_value());
    const auto f = encode(*m);
    REQUIRE(f.has_value());
    CHECK(f->len == 12);
    // decode still recovers the true body length from the length byte
    const auto out = decode(*f);
    REQUIRE(out.has_value());
    CHECK(out->body_len == 9);
}

TEST_CASE("make_stop is a severity stop carrying sender and a detail string") {
    const auto stop = make_stop(0, 0x12, "brake fault");
    const auto f    = encode(stop);
    REQUIRE(f.has_value());
    CHECK(f->id == CanId(MsgClass::Emergency, 0));  // severity 0 -> id 0x000

    const auto view = read_stop(stop);
    REQUIRE(view.has_value());
    CHECK(view->severity == 0);
    CHECK(view->sender == 0x12);
    REQUIRE(view->detail.size() == 11);
    CHECK(view->detail[0] == static_cast<std::uint8_t>('b'));
}

TEST_CASE("make_stop truncates an over-long detail but still transmits") {
    std::array<char, 200> big{};
    big.fill('x');
    const auto stop = make_stop(3, 0x40, std::string_view(big.data(), big.size()));
    CHECK(stop.type == MsgType::Stop);
    CHECK(stop.subaddress == 3);       // severity preserved
    CHECK(stop.body_len == kMaxBody);  // 1 sender + (kMaxBody - 1) detail

    const auto view = read_stop(stop);
    REQUIRE(view.has_value());
    CHECK(view->sender == 0x40);
    CHECK(view->detail.size() == kMaxBody - 1);
}

TEST_CASE("make_stop with an empty detail carries only the sender") {
    const auto stop = make_stop(1, 0x12, "");
    CHECK(stop.body_len == 1);

    const auto view = read_stop(stop);
    REQUIRE(view.has_value());
    CHECK(view->sender == 0x12);
    CHECK(view->detail.empty());
}

TEST_CASE("read_stop rejects a STOP with no sender byte") {
    const Message m{.cls = MsgClass::Emergency, .type = MsgType::Stop};
    CHECK_FALSE(read_stop(m).has_value());
}

TEST_CASE("read_stop rejects a message that is not a STOP") {
    const std::array<std::uint8_t, 1> body{0x01};
    const auto m = make_message(MsgClass::Telemetry, 0x10, MsgType::Update,
                                std::span<const std::uint8_t>(body));
    REQUIRE(m.has_value());
    CHECK_FALSE(read_stop(*m).has_value());
}

TEST_CASE("an empty body encodes to the two header bytes") {
    const Message m{
        .cls        = MsgClass::Command,
        .subaddress = 0x40,
        .type       = MsgType::Heartbeat,
    };
    const auto f = encode(m);
    REQUIRE(f.has_value());
    CHECK(f->id == CanId(MsgClass::Command, 0x40));
    CHECK(f->len == 2);
    CHECK(f->data[1] == 0);
}

TEST_CASE("an over-long body is rejected on build and encode") {
    const std::array<std::uint8_t, kMaxBody + 1> big{};
    CHECK_FALSE(make_message(MsgClass::Telemetry, 0, MsgType::Update, big).has_value());

    Message m{.body_len = kMaxBody + 1};
    CHECK_FALSE(encode(m).has_value());
}

TEST_CASE("make_message rejects an invalid class or type") {
    const std::array<std::uint8_t, 1> body{0x01};
    CHECK_FALSE(make_message(static_cast<MsgClass>(3), 0, MsgType::Update, body).has_value());
    CHECK_FALSE(make_message(MsgClass::Telemetry, 0, static_cast<MsgType>(5), body).has_value());
}

TEST_CASE("a frame shorter than the header is malformed") {
    CanFrame f{.len = 1};
    CHECK(decode(f).error() == DecodeError::BadLength);
}

TEST_CASE("a length byte larger than the frame is rejected") {
    CanFrame f{};
    f.data[1] = 20;  // claims 20 body bytes
    f.len     = 4;   // but only 2 are present
    CHECK(decode(f).error() == DecodeError::BodyOverrun);
}

TEST_CASE("decode rejects an id outside the 11-bit standard range") {
    CanFrame f{};
    f.id  = CanId::from_raw(0x800);  // would otherwise alias to Emergency/STOP via the class mask
    f.len = 2;
    CHECK(decode(f).error() == DecodeError::NonStandardId);
}

TEST_CASE("decode rejects a reserved class") {
    CanFrame f{};
    f.id  = CanId::from_raw(0x300);
    f.len = 2;
    CHECK(decode(f).error() == DecodeError::UnknownClass);
}

TEST_CASE("decode rejects an unknown type byte") {
    CanFrame f{};
    f.data[0] = 5;
    f.len     = 2;
    CHECK(decode(f).error() == DecodeError::UnknownType);
}

TEST_CASE("decode rejects a len that is not a CAN-FD size") {
    CanFrame f{};
    f.len = 9;
    CHECK(decode(f).error() == DecodeError::BadLength);
    f.len = 13;
    CHECK(decode(f).error() == DecodeError::BadLength);
}

TEST_CASE("body_view clamps a corrupt body_len instead of reading out of bounds") {
    Message m{};
    m.body_len = 200;  // past the 62-byte buffer
    CHECK(m.body_view().size() == kMaxBody);
}

TEST_CASE("decode rejects a nonsensical len past the FD payload size") {
    CanFrame f{};
    f.data[1] = 4;
    f.len     = 255;
    CHECK(decode(f).error() == DecodeError::BadLength);
}

TEST_CASE("a struct body round-trips through make_message and body_as") {
    const auto m = make_message(MsgClass::Telemetry, 0x10, MsgType::Update, WheelSpeed{1337});
    REQUIRE(m.has_value());
    REQUIRE(m->body_len == 2);
    // 1337 == 0x0539, little-endian on the wire
    CHECK(m->body[0] == 0x39);
    CHECK(m->body[1] == 0x05);

    const auto w = body_as<WheelSpeed>(*m);
    REQUIRE(w.has_value());
    CHECK(w->rpm == 1337);
}

TEST_CASE("body_as rejects a size mismatch") {
    const std::array<std::uint8_t, 1> one{0xAB};
    const auto m = make_message(MsgClass::Telemetry, 0x10, MsgType::Update,
                                std::span<const std::uint8_t>(one));
    REQUIRE(m.has_value());
    CHECK_FALSE(body_as<WheelSpeed>(*m).has_value());  // body is 1 byte, wants 2
}

TEST_CASE("struct make_message and body_as are constexpr") {
    constexpr auto m = make_message(MsgClass::Telemetry, 0x10, MsgType::Update, WheelSpeed{42});
    static_assert(m.has_value());
    static_assert(body_as<WheelSpeed>(*m)->rpm == 42);
    CHECK(true);
}

TEST_CASE("body state goes out as a 3-byte broadcast command on 0x108") {
    const BodyState s{static_cast<std::uint8_t>(BodyState::kHeadlights | BodyState::kLeftTurn)};
    const auto m = make_message(MsgClass::Command, kBodyStateId, MsgType::Update, s);
    REQUIRE(m.has_value());
    const auto f = encode(*m);
    REQUIRE(f.has_value());
    CHECK(f->id.raw() == 0x108);
    CHECK(f->len == 3);
    CHECK(f->data[0] == std::to_underlying(MsgType::Update));
    CHECK(f->data[1] == 1);
    CHECK(f->data[2] == 0x05);
}
