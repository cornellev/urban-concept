#include <doctest/doctest.h>

#include "chuds/ids.hpp"

using namespace chuds;

TEST_CASE("make_id packs class into the top bits and sub into the low byte") {
    CHECK(make_id(MsgClass::Emergency, 0x00) == 0x000);
    CHECK(make_id(MsgClass::Command, 0x40) == 0x140);
    CHECK(make_id(MsgClass::Telemetry, 0x10) == 0x210);
}

TEST_CASE("id_class and id_sub round-trip make_id") {
    const CanId id = make_id(MsgClass::Command, 0x7A);
    CHECK(id_class(id) == MsgClass::Command);
    CHECK(id_sub(id) == 0x7A);
}

TEST_CASE("class ranges are dense and gapless") {
    CHECK(class_min(MsgClass::Emergency) == 0x000);
    CHECK(class_max(MsgClass::Emergency) == 0x0FF);
    CHECK(class_min(MsgClass::Command) == 0x100);
    CHECK(class_max(MsgClass::Command) == 0x1FF);
    CHECK(class_min(MsgClass::Telemetry) == 0x200);
    CHECK(class_max(MsgClass::Telemetry) == 0x2FF);
}

TEST_CASE("priority order: emergency beats command beats telemetry") {
    CHECK(class_min(MsgClass::Emergency) < class_min(MsgClass::Command));
    CHECK(class_min(MsgClass::Command) < class_min(MsgClass::Telemetry));
}

TEST_CASE("every id stays within the 11-bit standard range") {
    CHECK(make_id(MsgClass::Telemetry, 0xFF) <= 0x7FF);
}

TEST_CASE("intent wrappers match make_id") {
    CHECK(emergency(0x03) == make_id(MsgClass::Emergency, 0x03));
    CHECK(command_to(0x40) == make_id(MsgClass::Command, 0x40));
    CHECK(telemetry_from(0x10) == make_id(MsgClass::Telemetry, 0x10));
}
