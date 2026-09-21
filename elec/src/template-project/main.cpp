#include <cstdio>

#include "pico/stdlib.h"

int main() {
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // blink and print
    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        printf("hello from template\n");
        sleep_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        sleep_ms(500);
    }
}
