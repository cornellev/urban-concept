#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "chuds/ids.hpp"

namespace chuds {

// the maximum CAN-FD data-field size in bytes
constexpr std::size_t kMaxFdPayload = 64;

// round a byte count (0..64) up to the next valid CAN-FD data length
[[nodiscard]] constexpr std::uint8_t round_up_dlc(std::uint8_t n) {
    if (n <= 8) return n;
    if (n <= 12) return 12;
    if (n <= 16) return 16;
    if (n <= 20) return 20;
    if (n <= 24) return 24;
    if (n <= 32) return 32;
    if (n <= 48) return 48;
    return 64;
}

// a raw CAN-FD frame, the unit the transport seam moves
// len is a physical CAN-FD data length (a valid DLC size), same meaning TX and RX
// always an FD frame with bit-rate switch and a standard 11-bit id
struct CanFrame {
    CanId id         = 0;
    std::uint8_t len = 0;
    std::array<std::uint8_t, kMaxFdPayload> data{};
};

}  // namespace chuds
