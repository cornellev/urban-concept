#include <cstdint>
#include <initializer_list>

#include "chuds/io.hpp"
#include "common/interval.hpp"
#include "common/mcp_bus.hpp"
#include "common/rpm.hpp"
#include "common/wire.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

using namespace cev::back_aux;

// brake sensor adc channel, derived from its gpio
static_assert(kBrakeAdc >= 26 && kBrakeAdc <= 29);
constexpr unsigned kBrakeChannel = kBrakeAdc - 26;

constexpr std::uint32_t kBlinkHalfPeriodMs = 333;
constexpr std::uint32_t kPublishPeriodMs   = 100;
// the leader must resend commands within this period or all outputs turn off
constexpr std::uint32_t kCommandTimeoutMs = 1000;

// servo pwm, 50 hz; center pulse and half-span at full deflection
constexpr std::uint16_t kWiperCenterUs   = 1500;
constexpr std::uint16_t kWiperHalfSpanUs = 500;

namespace {

// actuator state, driven by bus commands
struct Outputs {
    bool turn_left{};
    bool turn_right{};
};

// wiper sweep state, held apart from Outputs so a stop parks it instead of snapping
struct Wiper {
    int angle{kWiperParkDeg};
    int dir{1};
    bool running{};
    bool parking{};
    absolute_time_t dwell_until{};
};

void set_wiper_deg(int deg) {
    const auto us =
        static_cast<std::uint16_t>(kWiperCenterUs + deg * kWiperHalfSpanUs / kWiperMaxDeg);
    pwm_set_gpio_level(kWiperPwm, us);
}

// step one increment toward target, clamped
int step_toward(int from, int to) {
    if (from < to) {
        return from + kWiperStepDeg < to ? from + kWiperStepDeg : to;
    }
    if (from > to) {
        return from - kWiperStepDeg > to ? from - kWiperStepDeg : to;
    }
    return from;
}

// advance the sweep one tick; runs even while stopped so a park completes
void wiper_tick(Wiper& w) {
    if (w.parking) {
        if (w.angle != kWiperParkDeg) {
            w.angle = step_toward(w.angle, kWiperParkDeg);
            set_wiper_deg(w.angle);
        }
        return;
    }
    if (!w.running) {
        return;
    }
    if (absolute_time_diff_us(get_absolute_time(), w.dwell_until) > 0) {
        return;
    }
    w.angle += w.dir * kWiperStepDeg;
    if (w.angle >= kWiperMaxDeg) {
        w.angle       = kWiperMaxDeg;
        w.dir         = -1;
        w.dwell_until = make_timeout_time_ms(kWiperDwellMs);
    } else if (w.angle <= kWiperMinDeg) {
        w.angle       = kWiperMinDeg;
        w.dir         = 1;
        w.dwell_until = make_timeout_time_ms(kWiperDwellMs);
    }
    set_wiper_deg(w.angle);
}

void init_outputs() {
    for (unsigned pin : {kTurnLeft, kTurnRight}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, false);
    }
    // the car's running led, held on
    gpio_init(kRunningLed);
    gpio_set_dir(kRunningLed, GPIO_OUT);
    gpio_put(kRunningLed, true);

    // 1 us tick from clk_sys, 20000 ticks per frame gives 50 hz
    gpio_set_function(kWiperPwm, GPIO_FUNC_PWM);
    const unsigned slice = pwm_gpio_to_slice_num(kWiperPwm);
    pwm_set_clkdiv(slice, static_cast<float>(clock_get_hz(clk_sys)) / 1'000'000.0f);
    pwm_set_wrap(slice, 19999);
    pwm_set_enabled(slice, true);
    set_wiper_deg(kWiperParkDeg);
}

void all_off(Outputs& o, Wiper& w) {
    o = Outputs{};
    gpio_put(kTurnLeft, false);
    gpio_put(kTurnRight, false);
    w.running = false;
    w.parking = true;
}

void apply_command(Outputs& o, Wiper& w, const AuxCommand& c) {
    switch (c.actuator) {
        case Actuator::TurnLeft:
            o.turn_left = c.value != 0;
            if (!o.turn_left) {
                gpio_put(kTurnLeft, false);
            }
            break;
        case Actuator::TurnRight:
            o.turn_right = c.value != 0;
            if (!o.turn_right) {
                gpio_put(kTurnRight, false);
            }
            break;
        case Actuator::Wiper:
            w.running = c.value != 0;
            w.parking = c.value == 0;
            break;
        default: break;
    }
}

}  // namespace

// back_aux node: turn signals and wiper from commands, wheel speed and brake as telemetry
int main() {
    stdio_init_all();
    init_outputs();
    cev::rpm_init(kRpmLeft, kRpmRight);
    adc_init();
    adc_gpio_init(kBrakeAdc);

    cev::McpBus bus{{.sck  = kSpiSck,
                     .mosi = kSpiMosi,
                     .miso = kSpiMiso,
                     .cs   = kMcpCs,
                     .stby = kMcpStby,
                     .nint = kMcpInt}};

    Outputs out{};
    Wiper wiper{};
    // a stop latches until reset; todo clear on a ratified resume command
    bool stopped{};
    bool blink_on{};
    cev::Interval blink{kBlinkHalfPeriodMs};
    cev::Interval pub{kPublishPeriodMs};
    cev::Interval wiper_iv{kWiperTickMs};
    absolute_time_t command_deadline = make_timeout_time_ms(kCommandTimeoutMs);

    while (true) {
        while (true) {
            const auto rx = bus.recv();
            if (rx.status != chuds::RxStatus::Received || !rx.msg) {
                break;
            }
            const chuds::Message& m = *rx.msg;
            if (m.cls == chuds::MsgClass::Emergency && m.type == chuds::MsgType::Stop) {
                stopped = true;
                all_off(out, wiper);
            } else if (!stopped && m.cls == chuds::MsgClass::Command && m.subaddress == kNodeId &&
                       m.type == chuds::MsgType::Update) {
                if (auto c = chuds::body_as<AuxCommand>(m)) {
                    apply_command(out, wiper, *c);
                    command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
                }
            }
        }

        if (time_reached(command_deadline)) {
            all_off(out, wiper);
            command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
        }

        if (blink.due()) {
            blink_on = !blink_on;
            gpio_put(kTurnLeft, out.turn_left && blink_on);
            gpio_put(kTurnRight, out.turn_right && blink_on);
        }

        if (wiper_iv.due()) {
            wiper_tick(wiper);
        }

        if (pub.due()) {
            const cev::RpmCounts rpm = cev::rpm_take();
            adc_select_input(kBrakeChannel);
            bus.publish(kNodeId, Telemetry{rpm.left, rpm.right, adc_read()});
        }
    }
}
