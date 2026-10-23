#include <cmath>
#include <cstdio>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

// ---- pin map ----

struct BldcPins {
    uint pwm;
    uint brake;
    uint enable;
    uint dir;
};

struct StepperPins {
    uint pulse;
    uint dir;
    uint enable;
};

constexpr BldcPins BLDC[] = {
    {.pwm = 6, .brake = 7, .enable = 8, .dir = 9},
    {.pwm = 10, .brake = 11, .enable = 12, .dir = 13},
};
constexpr StepperPins STEPPER = {.pulse = 14, .dir = 15, .enable = 16};
constexpr uint LED_STRIP_PINS[] = {22, 23, 24};
constexpr uint ONBOARD_LED_PIN = 25;
constexpr uint ANALOG_PIN = 26;  // ADC0

constexpr uint BLDC_PWM_FREQ_HZ = 2'000;
constexpr float STEPPER_MAX_FREQ_HZ = 10'000.0f;  // DM542T pulse input limit
constexpr float STEPPER_FREQ = 2000.0f;  

int main() {
    
    stdio_init_all();

    gpio_init(STEPPER.enable);
    gpio_put(STEPPER.enable, 1);
    gpio_set_dir(STEPPER.enable, GPIO_OUT);

    gpio_init(STEPPER.dir);
    gpio_put(STEPPER.dir, 0);
    gpio_set_dir(STEPPER.dir, GPIO_OUT);

    gpio_init(ONBOARD_LED_PIN);
    gpio_set_dir(ONBOARD_LED_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(ANALOG_PIN);
    adc_select_input(0);

    const uint pin = STEPPER.pulse;
    uint slice = pwm_gpio_to_slice_num(pin);
    uint16_t wrap = clock_get_hz(clk_sys) / STEPPER_MAX_FREQ_HZ - 1;

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, wrap);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(pin, 0);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
    
    pwm_set_gpio_level(pin, static_cast<uint16_t>(0.5 * (wrap + 1)));

    while (true) {
        gpio_put(ONBOARD_LED_PIN, 1);
        printf("adc: %u\n", adc_read());
        sleep_ms(500);
        gpio_put(ONBOARD_LED_PIN, 0);
        sleep_ms(500);
    }
}
