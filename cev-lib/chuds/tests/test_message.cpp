#include <doctest/doctest.h>

#include <array>

#include "chuds/message.hpp"

using namespace chuds;

// encode and decode run at compile time (constexpr)
namespace {
constexpr Message kSample{
    .cls      = MsgClass::Command,
    .sub      = 0x40,
    .type     = MsgType::Update,
    .body     = {0x01, 0x1E},
    .body_len = 2,
};
constexpr auto kFrame = encode(kSample);
static_assert(kFrame.has_value());
static_assert(kFrame->id == 0x140);
static_assert(kFrame->len == 4);
constexpr auto kBack = decode(*kFrame);
static_assert(kBack.has_value());
static_assert(kBack->sub == 0x40);
static_assert(kBack->body_len == 2);
}  // namespace

TEST_CASE("encode lays out id, type, length, then body") {
    const auto f = encode(kSample);
    REQUIRE(f.has_value());
    CHECK(f->id == 0x140);
    CHECK(f->len == 4);
    CHECK(f->data[0] == static_cast<std::uint8_t>(MsgType::Update));
    CHECK(f->data[1] == 2);
    CHECK(f->data[2] == 0x01);
    CHECK(f->data[3] == 0x1E);
}

TEST_CASE("decode round-trips encode") {
    const Message m{
        .cls      = MsgClass::Telemetry,
        .sub      = 0x10,
        .type     = MsgType::Action,
        .body     = {0xAB},
        .body_len = 1,
    };
    const auto f = encode(m);
    REQUIRE(f.has_value());
    const auto out = decode(*f);
    REQUIRE(out.has_value());
    CHECK(out->cls == MsgClass::Telemetry);
    CHECK(out->sub == 0x10);
    CHECK(out->type == MsgType::Action);
    REQUIRE(out->body_len == 1);
    CHECK(out->body[0] == 0xAB);
}

TEST_CASE("decode recovers the true body length past CAN-FD padding") {
    CanFrame f{};
    f.id      = make_id(MsgClass::Telemetry, 0x10);
    f.data[0] = static_cast<std::uint8_t>(MsgType::Update);
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

TEST_CASE("make_stop is the global stop frame") {
    const auto f = encode(make_stop());
    REQUIRE(f.has_value());
    CHECK(f->id == kGlobalStop);
    const auto out = decode(*f);
    REQUIRE(out.has_value());
    CHECK(out->cls == MsgClass::Emergency);
    CHECK(out->type == MsgType::Stop);
}

TEST_CASE("empty body encodes to the two header bytes") {
    const Message m{
        .cls  = MsgClass::Emergency,
        .sub  = 0x00,
        .type = MsgType::Stop,
    };
    const auto f = encode(m);
    REQUIRE(f.has_value());
    CHECK(f->id == kGlobalStop);
    CHECK(f->len == 2);
    CHECK(f->data[1] == 0);
}

TEST_CASE("an over-long body is rejected on build and encode") {
    const std::array<std::uint8_t, kMaxBody + 1> big{};
    CHECK_FALSE(make_message(MsgClass::Telemetry, 0, MsgType::Update, big).has_value());

    Message m{.body_len = kMaxBody + 1};
    CHECK_FALSE(encode(m).has_value());
}

TEST_CASE("a frame shorter than the header is malformed") {
    CanFrame f{.len = 1};
    CHECK_FALSE(decode(f).has_value());
}

TEST_CASE("a length byte larger than the frame is rejected") {
    CanFrame f{};
    f.data[1] = 20;  // claims 20 body bytes
    f.len     = 4;   // but only 2 are present
    CHECK_FALSE(decode(f).has_value());
}

TEST_CASE("decode rejects an id outside the 11-bit standard range") {
    CanFrame f{};
    f.id  = 0x800;  // would otherwise alias to Emergency/STOP via the class mask
    f.len = 2;
    CHECK_FALSE(decode(f).has_value());
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
    CHECK_FALSE(decode(f).has_value());
}
