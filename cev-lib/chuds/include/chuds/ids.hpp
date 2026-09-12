#pragma once

#include <cstdint>

namespace chuds {

// 11-bit standard CAN identifier, 0x000-0x7FF
using CanId = std::uint16_t;

// top 3 bits of the id select the message class, low 8 bits are the subaddress
// classes are dense and ordered by priority, so the lower id wins arbitration
enum class MsgClass : std::uint8_t {
    Emergency = 0,
    Command = 1,
    Telemetry = 2,
};

constexpr int kClassShift = 8;
constexpr CanId kSubMask = 0x0FF;

// reserved global stop, the lowest id on the bus, always wins arbitration
constexpr CanId kGlobalStop = 0x000;

constexpr CanId make_id(MsgClass cls, std::uint8_t sub) {
    return static_cast<CanId>((static_cast<CanId>(cls) << kClassShift) | sub);
}

constexpr MsgClass id_class(CanId id) { return static_cast<MsgClass>((id >> kClassShift) & 0x07); }

constexpr std::uint8_t id_sub(CanId id) { return static_cast<std::uint8_t>(id & kSubMask); }

constexpr CanId class_min(MsgClass cls) { return make_id(cls, 0x00); }
constexpr CanId class_max(MsgClass cls) { return make_id(cls, 0xFF); }

// intent-revealing wrappers, one per class, mirroring the subaddress meaning
constexpr CanId emergency(std::uint8_t reason) { return make_id(MsgClass::Emergency, reason); }
constexpr CanId command_to(std::uint8_t device) { return make_id(MsgClass::Command, device); }
constexpr CanId telemetry_from(std::uint8_t sensor) { return make_id(MsgClass::Telemetry, sensor); }

}  // namespace chuds
