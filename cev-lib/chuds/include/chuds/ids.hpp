#pragma once

#include <cstdint>

namespace chuds {

// 11-bit standard CAN identifier, 0x000-0x7FF
using CanId = std::uint16_t;

// top 3 bits of the id select the message class, low 8 bits are the subaddress
// classes are dense and ordered by priority, so the lower id wins arbitration
enum class MsgClass : std::uint8_t {
    Emergency = 0,
    Command   = 1,
    Telemetry = 2,
};

constexpr int kClassShift  = 8;
constexpr CanId kClassMask = 0x07;
constexpr CanId kSubMask   = 0x0FF;

// largest valid 11-bit standard identifier
constexpr CanId kMaxStandardId = 0x7FF;

// reserved global stop, the lowest id on the bus, always wins arbitration
constexpr CanId kGlobalStop = 0x000;

// emergency subaddress 0 is reserved for the global stop; fault reasons start at 1
constexpr std::uint8_t kStopReason = 0;

[[nodiscard]] constexpr CanId make_id(MsgClass cls, std::uint8_t sub) {
    return static_cast<CanId>((static_cast<CanId>(cls) << kClassShift) | sub);
}

[[nodiscard]] constexpr MsgClass id_class(CanId id) {
    return static_cast<MsgClass>((id >> kClassShift) & kClassMask);
}

[[nodiscard]] constexpr std::uint8_t id_sub(CanId id) {
    return static_cast<std::uint8_t>(id & kSubMask);
}

[[nodiscard]] constexpr CanId class_min(MsgClass cls) { return make_id(cls, 0x00); }
[[nodiscard]] constexpr CanId class_max(MsgClass cls) { return make_id(cls, 0xFF); }

// intent-revealing wrappers, one per class, mirroring the subaddress meaning
// reason 0 is reserved for the global stop, pass 1-255 for a fault
[[nodiscard]] constexpr CanId emergency(std::uint8_t reason) {
    return make_id(MsgClass::Emergency, reason);
}
[[nodiscard]] constexpr CanId command_to(std::uint8_t device) {
    return make_id(MsgClass::Command, device);
}
[[nodiscard]] constexpr CanId telemetry_from(std::uint8_t sensor) {
    return make_id(MsgClass::Telemetry, sensor);
}

}  // namespace chuds
