#pragma once

#include <cstdint>

#include "pico/time.h"

namespace cev {

// a fixed-period tick
// due() returns true once per period and advances the deadline
// a slightly late tick keeps the original phase so the period does not drift
// a stall longer than a period skips the missed ticks instead of firing them back to back
struct Interval {
    std::uint32_t period_ms{};
    absolute_time_t next = make_timeout_time_ms(period_ms);

    [[nodiscard]] bool due() {
        const absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(now, next) > 0) {
            return false;
        }
        next = delayed_by_ms(next, period_ms);
        if (absolute_time_diff_us(now, next) <= 0) {
            next = delayed_by_ms(now, period_ms);
        }
        return true;
    }
};

}  // namespace cev
