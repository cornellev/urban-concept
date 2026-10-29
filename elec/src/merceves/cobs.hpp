#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// COBS (consistent overhead byte stuffing): an encoded frame never contains 0x00,
// so a 0x00 byte can mark where each frame ends on a byte stream.
namespace cobs {

// worst-case encoded size of n bytes, without the trailing 0x00
constexpr size_t max_encoded_size(size_t n) { return n + n / 254 + 1; }

// out needs max_encoded_size(in.size()) bytes. returns the encoded length, without the trailing 0x00
inline size_t encode(std::span<const uint8_t> in, std::span<uint8_t> out) {
    size_t code_at = 0;
    size_t written = 1;
    uint8_t code = 1;
    for (const uint8_t byte : in) {
        if (byte != 0) {
            out[written++] = byte;
            code++;
        }
        if (byte == 0 || code == 0xFF) {
            out[code_at] = code;
            code_at = written++;
            code = 1;
        }
    }
    out[code_at] = code;
    return written;
}

// in is one frame without its trailing 0x00. returns the decoded length,
// or nullopt if the frame is malformed or doesn't fit in out
[[nodiscard]] inline std::optional<size_t> decode(std::span<const uint8_t> in, std::span<uint8_t> out) {
    size_t read = 0;
    size_t written = 0;
    while (read < in.size()) {
        const uint8_t code = in[read++];
        if (code == 0 || read + code - 1 > in.size()) {
            return std::nullopt;
        }
        for (uint8_t i = 1; i < code; i++) {
            if (in[read] == 0 || written == out.size()) {
                return std::nullopt;
            }
            out[written++] = in[read++];
        }
        if (code < 0xFF && read < in.size()) {
            if (written == out.size()) {
                return std::nullopt;
            }
            out[written++] = 0;
        }
    }
    return written;
}

}  // namespace cobs
