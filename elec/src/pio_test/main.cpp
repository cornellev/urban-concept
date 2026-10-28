#include <cstdio>

#include "common/rpm.hpp"
#include "config.hpp"
#include "pico/platform.h"
#include "pico/stdlib.h"

namespace {

void pulses(unsigned n, unsigned half_period_cycles) {
    for (unsigned i = 0; i < n; ++i) {
        gpio_put(kPulsePin, true);
        busy_wait_at_least_cycles(half_period_cycles);
        gpio_put(kPulsePin, false);
        busy_wait_at_least_cycles(half_period_cycles);
    }
}

}  // namespace

// checks rpm.pio: drives pulses out of kPulsePin and prints whether kCountPin counted each one
int main() {
    stdio_init_all();
    gpio_init(kPulsePin);
    gpio_set_dir(kPulsePin, GPIO_OUT);
    gpio_put(kPulsePin, false);

    cev::rpm_init(kCountPin, kSparePin);
    sleep_ms(3000);

    for (unsigned round = 0;; ++round) {
        (void)cev::rpm_take();

        const unsigned n = 100 * (round % 10 + 1);
        // slwo and fast pulses
        const unsigned half = round % 2 == 0 ? 625 : 25;
        pulses(n, half);
        const cev::RpmCounts idle_check = cev::rpm_take();
        sleep_ms(200);
        const cev::RpmCounts after_idle = cev::rpm_take();

        std::printf("round %u half_cycles %u sent %u counted %u spare %u idle_extra %u %s\n", round,
                    half, n, idle_check.left, idle_check.right, after_idle.left,
                    (idle_check.left == n && after_idle.left == 0 && idle_check.right == 0)
                        ? "OK"
                        : "FAIL");
        sleep_ms(800);
    }
}
