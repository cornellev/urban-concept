#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>

#include "chuds/mcp_transport.hpp"
#include "common/can_health.hpp"
#include "common/interval.hpp"
#include "config.hpp"
#include "pico/stdlib.h"

constexpr std::uint32_t kBlinkHalfPeriodMs = 500;
// frames handled per loop pass, the mcp rx fifo depth, so a flooded bus cannot starve the timers
constexpr int kMaxRxPerPass = 8;
// "7FF##1" plus 64 bytes as hex
constexpr std::size_t kMaxLine = 6 + 2 * chuds::kMaxFdPayload;

// value of one hex digit, or -1
int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// parse a cansend-style fd line, "123##1DEADBEEF", flags digit ignored since the mcp always sends with brs
std::optional<chuds::CanFrame> parse_line(const std::array<char, kMaxLine>& line, std::size_t n) {
    if (n < 6 || line[3] != '#' || line[4] != '#' || hex_digit(line[5]) < 0 || (n - 6) % 2 != 0) {
        return std::nullopt;
    }
    int id = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        const int d = hex_digit(line[i]);
        if (d < 0) {
            return std::nullopt;
        }
        id = id * 16 + d;
    }
    if (id > chuds::kMaxStandardId.raw()) {
        return std::nullopt;
    }
    chuds::CanFrame f{};
    f.id  = chuds::CanId::from_raw(static_cast<std::uint16_t>(id));
    f.len = static_cast<std::uint8_t>((n - 6) / 2);
    for (std::size_t i = 0; i < f.len; ++i) {
        const int hi = hex_digit(line[6 + 2 * i]);
        const int lo = hex_digit(line[7 + 2 * i]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        f.data[i] = static_cast<std::uint8_t>(hi * 16 + lo);
    }
    return f;
}

// serial bridge: every frame from the bus goes out as a line on usb serial, and every line in goes onto the bus
// lines that are not frames are status text for a human
int main() {
    stdio_init_all();

    gpio_init(kStatusLed);
    gpio_set_dir(kStatusLed, GPIO_OUT);

    chuds::Mcp251863Transport can{{
        .sck  = kSpiSck,
        .mosi = kSpiMosi,
        .miso = kSpiMiso,
        .cs   = kMcpCs,
        .stby = kMcpStby,
        .nint = kMcpInt,
    }};

    bool led_on{};
    std::array<char, kMaxLine> line{};
    std::size_t line_len = 0;
    bool line_too_long{};
    cev::Interval blink{kBlinkHalfPeriodMs};
    cev::Interval report{1000};
    cev::CanHealth can_health;

    while (true) {
        can_health.check(can);

        for (int i = 0; i < kMaxRxPerPass; ++i) {
            const auto rx = can.recv();
            if (!rx) {
                break;
            }
            std::printf("%03X##1", static_cast<unsigned>(rx->id.raw()));
            for (std::size_t j = 0; j < rx->len; ++j) {
                std::printf("%02X", static_cast<unsigned>(rx->data[j]));
            }
            std::printf("\n");
        }

        for (int c = getchar_timeout_us(0); c != PICO_ERROR_TIMEOUT; c = getchar_timeout_us(0)) {
            if (c == '\r') {
                continue;
            }
            if (c != '\n') {
                if (line_len < line.size()) {
                    line[line_len++] = static_cast<char>(c);
                } else {
                    line_too_long = true;
                }
                continue;
            }
            const auto f = line_too_long ? std::nullopt : parse_line(line, line_len);
            if (!f) {
                std::printf("bad line\n");
            } else if (!can.send(*f)) {
                std::printf("can tx failed\n");
            }
            line_len      = 0;
            line_too_long = false;
        }

        // repeated so a serial console opened after boot still sees it
        if (!can.ready() && report.due()) {
            std::printf("can init failed\n");
        }

        if (blink.due()) {
            led_on = !led_on;
            gpio_put(kStatusLed, led_on);
        }
    }
}
