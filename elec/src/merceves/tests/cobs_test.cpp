// Run: g++ -std=c++20 -Wall -Wextra -Werror tests/cobs_test.cpp -o /tmp/cobs_test && /tmp/cobs_test
#include "../cobs.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

static std::vector<uint8_t> encode(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out(cobs::max_encoded_size(in.size()));
    out.resize(cobs::encode(in, out));
    return out;
}

static void round_trip(const std::vector<uint8_t>& in) {
    const auto encoded = encode(in);
    for (const uint8_t byte : encoded) {
        assert(byte != 0);
    }
    std::vector<uint8_t> decoded(encoded.size());
    const auto len = cobs::decode(encoded, decoded);
    assert(len && *len == in.size());
    decoded.resize(*len);
    assert(decoded == in);
}

int main() {
    // known vectors from the COBS paper and wikipedia
    assert(encode({}) == std::vector<uint8_t>({0x01}));
    assert(encode({0x00}) == std::vector<uint8_t>({0x01, 0x01}));
    assert(encode({0x11, 0x22, 0x00, 0x33}) == std::vector<uint8_t>({0x03, 0x11, 0x22, 0x02, 0x33}));

    round_trip({});
    round_trip({0x00, 0x00, 0x00});
    round_trip(std::vector<uint8_t>(254, 0x07));  // exactly one full block
    std::vector<uint8_t> mixed;
    for (int i = 1; i <= 600; i++) {
        mixed.push_back(static_cast<uint8_t>(i));  // wraps through 0x00 twice
    }
    round_trip(mixed);

    // malformed input is rejected, never read or written out of bounds
    std::vector<uint8_t> out(8);
    assert(!cobs::decode(std::vector<uint8_t>({0x05, 0x11}), out));  // code runs past the end
    assert(!cobs::decode(std::vector<uint8_t>({0x02, 0x00}), out));   // stray 0x00 inside a frame
    std::vector<uint8_t> tiny(2);
    assert(!cobs::decode(encode({1, 2, 3, 4}), tiny));  // doesn't fit in out

    std::puts("cobs tests passed");
}
