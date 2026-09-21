#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "chuds/io.hpp"
#include "common/mcp_bus.hpp"
#include "common/rpm.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

// this node's id: telemetry source and command recipient
constexpr std::uint8_t kNodeId = 0x20;

// brake sensor adc channel, derived from its gpio
constexpr unsigned kBrakeChannel = kBrakeAdc - 26;

constexpr std::uint32_t kBlinkHalfPeriodMs = 333;
constexpr std::uint32_t kPublishPeriodMs   = 100;

// servo pwm pulse range, 50 hz
constexpr std::uint16_t kWiperMinUs = 1000;
constexpr std::uint16_t kWiperMaxUs = 2000;

namespace {

// telemetry body, published once per cycle
struct Telemetry {
    std::uint16_t rpm_left;   // wheel edges in the last window
    std::uint16_t rpm_right;  // wheel edges in the last window
    std::uint16_t brake;      // raw adc, 0-4095
};
static_assert(sizeof(Telemetry) == 6);

enum class Actuator : std::uint8_t {
    TurnLeft,
    TurnRight,
    Wiper,
};

// command body: set one actuator from the bus
// value is on/off for the turn signals, a 0-255 position for the wiper
struct AuxCommand {
    Actuator actuator;
    std::uint8_t value;
};
static_assert(sizeof(AuxCommand) == 2);

// actuator state, driven by bus commands
struct Outputs {
    bool turn_left{};
    bool turn_right{};
    std::uint8_t wiper{};
};

void set_wiper(std::uint8_t level) {
    const std::uint16_t us =
        kWiperMinUs + static_cast<std::uint16_t>((kWiperMaxUs - kWiperMinUs) * level / 255);
    pwm_set_gpio_level(kWiperPwm, us);
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

    // 1 us tick (125 mhz / 125), 20000 us period gives 50 hz
    gpio_set_function(kWiperPwm, GPIO_FUNC_PWM);
    const unsigned slice = pwm_gpio_to_slice_num(kWiperPwm);
    pwm_set_clkdiv(slice, 125.0f);
    pwm_set_wrap(slice, 20000);
    pwm_set_enabled(slice, true);
    set_wiper(0);
}

void all_off(Outputs& o) {
    o = Outputs{};
    gpio_put(kTurnLeft, false);
    gpio_put(kTurnRight, false);
    set_wiper(0);
}

void apply_command(Outputs& o, const AuxCommand& c) {
    switch (c.actuator) {
        case Actuator::TurnLeft: o.turn_left = c.value != 0; break;
        case Actuator::TurnRight: o.turn_right = c.value != 0; break;
        case Actuator::Wiper:
            o.wiper = c.value;
            set_wiper(o.wiper);
            break;
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

    cev::McpBus bus{{kSpiSck, kSpiMosi, kSpiMiso, kMcpCs, kMcpStby}};
    auto& tx = bus.transport();

    // latched so a persistent fault prints once, not every loop
    bool bus_ok = bus.ok();

    Outputs out{};
    // a stop latches until reset; todo clear on a ratified resume command
    bool stopped{};
    bool blink_on{};
    absolute_time_t next_blink = make_timeout_time_ms(kBlinkHalfPeriodMs);
    absolute_time_t next_pub   = make_timeout_time_ms(kPublishPeriodMs);

    while (true) {
        auto rx = chuds::recv(tx);
        if (bus_ok &&
            (rx.status == chuds::RxStatus::BusOff || rx.status == chuds::RxStatus::Overflow)) {
            std::printf("can rx fault\n");
            bus_ok = false;
        }
        if (rx.status == chuds::RxStatus::Received && rx.msg) {
            const chuds::Message& m = *rx.msg;
            if (m.cls == chuds::MsgClass::Emergency && m.type == chuds::MsgType::Stop) {
                stopped = true;
                all_off(out);
            } else if (!stopped && m.cls == chuds::MsgClass::Command && m.subaddress == kNodeId) {
                if (auto c = chuds::body_as<AuxCommand>(m)) {
                    apply_command(out, *c);
                }
            }
        }

        if (absolute_time_diff_us(get_absolute_time(), next_blink) <= 0) {
            blink_on   = !blink_on;
            next_blink = delayed_by_ms(next_blink, kBlinkHalfPeriodMs);
            gpio_put(kTurnLeft, out.turn_left && blink_on);
            gpio_put(kTurnRight, out.turn_right && blink_on);
        }

        if (absolute_time_diff_us(get_absolute_time(), next_pub) <= 0) {
            next_pub                 = delayed_by_ms(next_pub, kPublishPeriodMs);
            const cev::RpmCounts rpm = cev::rpm_take();
            adc_select_input(kBrakeChannel);
            const Telemetry t{rpm.left, rpm.right, adc_read()};
            if (auto m = chuds::make_message(chuds::MsgClass::Telemetry, kNodeId,
                                             chuds::MsgType::Update, t)) {
                const chuds::TxStatus st = chuds::send(tx, *m);
                if (bus_ok && (st == chuds::TxStatus::BusOff || st == chuds::TxStatus::Error)) {
                    std::printf("can tx fault\n");
                    bus_ok = false;
                }
            }
        }
    }
}
