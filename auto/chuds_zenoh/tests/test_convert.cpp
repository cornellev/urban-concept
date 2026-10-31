#include <doctest/doctest.h>

#include "convert.hpp"

using chuds_zenoh::parse_control;

TEST_CASE("parse_control reads steering angle and velocity") {
    const auto cmd = parse_control(R"({"steering_angle": -12.5, "velocity": 1.25})");
    REQUIRE(cmd.has_value());
    CHECK(cmd->steering_angle == -12.5);
    CHECK(cmd->velocity == 1.25);
}

TEST_CASE("parse_control accepts integers and ignores extra fields") {
    const auto cmd = parse_control(R"({"steering_angle": 3, "velocity": 0, "mode": "auto"})");
    REQUIRE(cmd.has_value());
    CHECK(cmd->steering_angle == 3.0);
    CHECK(cmd->velocity == 0.0);
}

TEST_CASE("parse_control rejects a payload that is not a control command") {
    CHECK_FALSE(parse_control("not json").has_value());
    CHECK_FALSE(parse_control("[1, 2]").has_value());
    CHECK_FALSE(parse_control(R"({"steering_angle": 1.0})").has_value());
    CHECK_FALSE(parse_control(R"({"velocity": 1.0})").has_value());
    CHECK_FALSE(parse_control(R"({"steering_angle": "1", "velocity": 1.0})").has_value());
    CHECK_FALSE(parse_control(R"({"steering_angle": 1.0, "velocity": null})").has_value());
    // pure_pursuit's field name
    CHECK_FALSE(parse_control(R"({"steering_angle": 1.0, "speed": 1.0})").has_value());
}

TEST_CASE("telemetry_payloads splits one telemetry body into three topics") {
    const auto out = chuds_zenoh::telemetry_payloads({3.5, 3.75, -12.0});
    CHECK(out.left_rpm == R"({"rpm":3.5})");
    CHECK(out.right_rpm == R"({"rpm":3.75})");
    CHECK(out.steering_angle == R"({"steering_angle":-12.0})");
}

// TODO: test the imu zenoh conversion once a source fills ImuSample (units, scaling, axis signs)
