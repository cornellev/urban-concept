#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "chuds/catalog.hpp"

namespace chuds_zenoh {

// a cev/commands/control payload: {"steering_angle": deg, "velocity": m/s}
// nullopt if it is not json, or a field is missing or not a number
[[nodiscard]] inline std::optional<chuds::merceves::DriveCommand> parse_control(
    std::string_view payload) {
    const auto j = nlohmann::json::parse(payload, nullptr, false);
    if (!j.is_object()) {
        return std::nullopt;
    }
    const auto steering = j.find("steering_angle");
    const auto velocity = j.find("velocity");
    if (steering == j.end() || velocity == j.end() || !steering->is_number() ||
        !velocity->is_number()) {
        return std::nullopt;
    }
    return chuds::merceves::DriveCommand{
        .steering_angle = steering->get<double>(),
        .velocity       = velocity->get<double>(),
    };
}

// one json payload per telemetry topic
struct TelemetryPayloads {
    std::string left_rpm;        // {"rpm": rotations/s}
    std::string right_rpm;       // {"rpm": rotations/s}
    std::string steering_angle;  // {"steering_angle": deg}
};

[[nodiscard]] inline TelemetryPayloads telemetry_payloads(const chuds::merceves::Telemetry& t) {
    return {
        .left_rpm       = nlohmann::json{{"rpm", t.rpm_left}}.dump(),
        .right_rpm      = nlohmann::json{{"rpm", t.rpm_right}}.dump(),
        .steering_angle = nlohmann::json{{"steering_angle", t.steering_angle}}.dump(),
    };
}

// one 6dof imu reading
struct ImuSample {
    double accel_x{};     // m/s^2
    double accel_y{};     // m/s^2
    double accel_z{};     // m/s^2
    double roll_rate{};   // rad/s
    double pitch_rate{};  // rad/s
    double yaw_rate{};    // rad/s
};

[[nodiscard]] inline std::string imu_payload(const ImuSample& s) {
    return nlohmann::json{
        {"accel_x", s.accel_x},     {"accel_y", s.accel_y},       {"accel_z", s.accel_z},
        {"roll_rate", s.roll_rate}, {"pitch_rate", s.pitch_rate}, {"yaw_rate", s.yaw_rate},
    }
        .dump();
}

}  // namespace chuds_zenoh
