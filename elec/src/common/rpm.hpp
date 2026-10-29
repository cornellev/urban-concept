#pragma once

#include <stdint.h>

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "rpm.pio.h"

namespace cev {

// one pio0 state machine per wheel-speed input counts its rising edges, see rpm.pio
struct RpmCounter {
    // pio0 state machine index, 0-3
    unsigned sm;
    // edge total at the previous read
    uint32_t last;
};

inline RpmCounter g_rpm_left{};
inline RpmCounter g_rpm_right{};

inline void rpm_counter_init(RpmCounter& counter, unsigned offset, unsigned pin) {
    // reserve a free sm, panic if none
    counter.sm = pio_claim_unused_sm(pio0, true);
    // pin is an input
    pio_sm_set_consecutive_pindirs(pio0, counter.sm, pin, 1, false);
    // hand pin to pio0
    pio_gpio_init(pio0, pin);
    // weak, the divider still holds it low
    gpio_pull_up(pin);
    // program location and wrap
    pio_sm_config cfg = rpm_edge_counter_program_get_default_config(offset);
    // pin that JMP pin tests
    sm_config_set_jmp_pin(&cfg, pin);
    // apply config, pc = program start
    pio_sm_init(pio0, counter.sm, offset, &cfg);
    // x = 0
    pio_sm_exec(pio0, counter.sm, pio_encode_set(pio_x, 0));
    // matches x = 0
    counter.last = 0;
    // start counting
    pio_sm_set_enabled(pio0, counter.sm, true);
}

// edges since the previous call
inline uint16_t rpm_counter_take(RpmCounter& counter) {
    // the sm pushes its count every few cycles, faster than the cpu can pop, so the fifo is never
    // empty and holds whatever was pushed when it last filled. drop that, then take the next push,
    // which is current and arrives within a few cycles
    pio_sm_clear_fifos(pio0, counter.sm);
    const uint32_t total = 0u - pio_sm_get_blocking(pio0, counter.sm);

    const uint32_t edges = total - counter.last;
    counter.last         = total;

    // telemetry field is 16 bits
    return static_cast<uint16_t>(edges);
}

// count rising edges on two wheel-speed inputs
// inputs expect a pnp sensor driving 12v through the board's divider, which also holds the pin low
inline void rpm_init(unsigned left_pin, unsigned right_pin) {
    // load once, both sms run the same program
    const unsigned offset = pio_add_program(pio0, &rpm_edge_counter_program);
    rpm_counter_init(g_rpm_left, offset, left_pin);
    rpm_counter_init(g_rpm_right, offset, right_pin);
}

struct RpmCounts {
    uint16_t left;
    uint16_t right;
};

// edges since the previous call, then reset
[[nodiscard]] inline RpmCounts rpm_take() {
    return {rpm_counter_take(g_rpm_left), rpm_counter_take(g_rpm_right)};
}

}  // namespace cev
