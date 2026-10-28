#include <cstdio>

#include "common/rpm.hpp"
#include "config.hpp"
#include "pico/stdlib.h"

// reads the wheel-speed sensor on kRpmPin and prints edges per second on USB serial
int main() {
    stdio_init_all();

    cev::rpm_init(kRpmPin, kSparePin);
    sleep_ms(3000);

    for (;;) {
        const cev::RpmCounts rpm = cev::rpm_take();
        std::printf("edges %u\n", rpm.left);
        sleep_ms(100);
    }
}
