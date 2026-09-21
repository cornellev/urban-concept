#include <doctest/doctest.h>

#include "chuds/ids.hpp"

using namespace chuds;

TEST_CASE("CanId packs class into the top bits and sub into the low byte") {
    CHECK(CanId(MsgClass::Emergency, 0x00).raw() == 0x000);
    CHECK(CanId(MsgClass::Command, 0x40).raw() == 0x140);
    CHECK(CanId(MsgClass::Telemetry, 0x10).raw() == 0x210);
}

TEST_CASE("cls and subaddress round-trip the constructor") {
    const CanId id = CanId(MsgClass::Command, 0x7A);
    CHECK(id.cls() == MsgClass::Command);
    CHECK(id.subaddress() == 0x7A);
}

TEST_CASE("class ranges are dense and gapless") {
    CHECK(CanId(MsgClass::Emergency, 0x00).raw() == 0x000);
    CHECK(class_max(MsgClass::Emergency).raw() == 0x0FF);
    CHECK(CanId(MsgClass::Command, 0x00).raw() == 0x100);
    CHECK(class_max(MsgClass::Command).raw() == 0x1FF);
    CHECK(CanId(MsgClass::Telemetry, 0x00).raw() == 0x200);
    CHECK(class_max(MsgClass::Telemetry).raw() == 0x2FF);
}

TEST_CASE("priority order: emergency beats command beats telemetry") {
    CHECK(CanId(MsgClass::Emergency, 0x00) < CanId(MsgClass::Command, 0x00));
    CHECK(CanId(MsgClass::Command, 0x00) < CanId(MsgClass::Telemetry, 0x00));
}

TEST_CASE("every id stays within the 11-bit standard range") {
    CHECK(CanId(MsgClass::Telemetry, 0xFF) <= kMaxStandardId);
}
