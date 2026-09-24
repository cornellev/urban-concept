#pragma once

#include <cstdint>

namespace cev::front_aux {

// this node's id: telemetry source and command recipient
constexpr std::uint8_t kNodeId = 0x10;

// telemetry body, published once per cycle
struct Telemetry {
    std::uint16_t rpm_left;   // wheel edges in the last window
    std::uint16_t rpm_right;  // wheel edges in the last window
    std::uint16_t steering;   // raw adc, 0-4095
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(Telemetry) == 6);

enum class Actuator : std::uint8_t {
    TurnLeft,
    TurnRight,
    Headlights,
    Horn,
};

// command body: set one actuator from the bus
// value is on/off
struct AuxCommand {
    Actuator actuator;
    std::uint8_t value;
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(AuxCommand) == 2);

}  // namespace cev::front_aux

namespace cev::back_aux {

// this node's id: telemetry source and command recipient
constexpr std::uint8_t kNodeId = 0x20;

// telemetry body, published once per cycle
struct Telemetry {
    std::uint16_t rpm_left;   // wheel edges in the last window
    std::uint16_t rpm_right;  // wheel edges in the last window
    std::uint16_t brake;      // raw adc, 0-4095
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(Telemetry) == 6);

enum class Actuator : std::uint8_t {
    TurnLeft,
    TurnRight,
    Wiper,
};

// command body: set one actuator from the bus
// value is on/off
// the wiper runs its own sweep while on
struct AuxCommand {
    Actuator actuator;
    std::uint8_t value;
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(AuxCommand) == 2);

}  // namespace cev::back_aux

namespace cev::joulemeter {

// this node's id: telemetry source
constexpr std::uint8_t kNodeId = 0x30;

// telemetry body, published every report period
struct Telemetry {
    float voltage;  // bus volts, averaged over the report period
    float current;  // bus amps, averaged over the report period
    float power;    // watts, averaged over the report period
    float energy;   // joules since boot
    static constexpr bool chuds_wire_body = true;
};
static_assert(sizeof(Telemetry) == 16);

}  // namespace cev::joulemeter
