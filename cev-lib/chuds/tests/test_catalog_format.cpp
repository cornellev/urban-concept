#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string_view>

#include "chuds/catalog.hpp"
#include "chuds/catalog_format.hpp"
#include "chuds/message.hpp"

using namespace chuds;

TEST_CASE("format_message prints a catalog body with its values") {
    const auto m = make_message(MsgClass::Command, kBodyStateId, MsgType::Update,
                                BodyState{BodyState::kHeadlights});
    REQUIRE(m.has_value());
    std::array<char, 96> line{};
    const int n = format_message(*m, line);
    CHECK(std::string_view(line.data()) == "body state 0x04");
    CHECK(n == 15);
}

TEST_CASE("format_message prints joulemeter floats") {
    const joulemeter::Telemetry t{47.91f, 3.2f, 153.3f, 812.4f};
    const auto m = make_message(MsgClass::Telemetry, joulemeter::kNodeId, MsgType::Update, t);
    REQUIRE(m.has_value());
    std::array<char, 96> line{};
    format_message(*m, line);
    CHECK(std::string_view(line.data()) == "joulemeter 47.91 V 3.20 A 153.3 W 812.4 J");
}

TEST_CASE("format_message falls back to the header for a message outside the catalog") {
    const std::array<std::uint8_t, 2> body{0x01, 0x02};
    const auto m = make_message(MsgClass::Command, 0x42, MsgType::Update, body);
    REQUIRE(m.has_value());
    std::array<char, 96> line{};
    format_message(*m, line);
    CHECK(std::string_view(line.data()) == "class 1 subaddress 0x42 type 0, 2 body bytes");
}

TEST_CASE("format_message truncates to the buffer and reports the full length") {
    const auto m = make_message(MsgClass::Command, kBodyStateId, MsgType::Update, BodyState{});
    REQUIRE(m.has_value());
    std::array<char, 5> line{};
    const int n = format_message(*m, line);
    CHECK(std::string_view(line.data()) == "body");
    CHECK(n == 15);
}
