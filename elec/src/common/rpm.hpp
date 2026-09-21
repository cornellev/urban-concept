#pragma once

#include <atomic>
#include <cstdint>
#include <initializer_list>

#include "hardware/gpio.h"

namespace cev {

// wheel-speed edge counters, shared with the gpio irq
inline std::atomic<std::uint32_t> g_rpm_left{0};
inline std::atomic<std::uint32_t> g_rpm_right{0};
inline unsigned g_rpm_left_pin  = 0;
inline unsigned g_rpm_right_pin = 0;

inline void rpm_on_edge(unsigned gpio, std::uint32_t /*events*/) {
    if (gpio == g_rpm_left_pin) {
        g_rpm_left.fetch_add(1, std::memory_order_relaxed);
    } else if (gpio == g_rpm_right_pin) {
        g_rpm_right.fetch_add(1, std::memory_order_relaxed);
    }
}

// count rising edges on two wheel-speed inputs
// claims the core's single gpio irq callback
inline void rpm_init(unsigned left_pin, unsigned right_pin) {
    g_rpm_left_pin  = left_pin;
    g_rpm_right_pin = right_pin;
    for (unsigned pin : {left_pin, right_pin}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
    }
    gpio_set_irq_enabled_with_callback(left_pin, GPIO_IRQ_EDGE_RISE, true, &rpm_on_edge);
    gpio_set_irq_enabled(right_pin, GPIO_IRQ_EDGE_RISE, true);
}

struct RpmCounts {
    std::uint16_t left;
    std::uint16_t right;
};

// edges since the previous call, then reset
inline RpmCounts rpm_take() {
    return {static_cast<std::uint16_t>(g_rpm_left.exchange(0, std::memory_order_relaxed)),
            static_cast<std::uint16_t>(g_rpm_right.exchange(0, std::memory_order_relaxed))};
}

}  // namespace cev
