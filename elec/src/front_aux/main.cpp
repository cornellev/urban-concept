#include <cstdint>
#include <initializer_list>

#include "chuds/io.hpp"
#include "common/interval.hpp"
#include "common/mcp_bus.hpp"
#include "common/rpm.hpp"
#include "common/wire.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "pico/stdlib.h"

using namespace cev::front_aux;

// steering sensor adc channel, derived from its gpio
static_assert(kSteeringAdc >= 26 && kSteeringAdc <= 29);
constexpr unsigned kSteeringChannel = kSteeringAdc - 26;

constexpr std::uint32_t kBlinkHalfPeriodMs = 333;
constexpr std::uint32_t kPublishPeriodMs   = 100;
// the leader must resend commands within this period or all outputs turn off
constexpr std::uint32_t kCommandTimeoutMs = 1000;

namespace {

// actuator state, driven by bus commands
struct Outputs {
    bool turn_left{};
    bool turn_right{};
    bool headlights{};
    bool horn{};
};

void init_outputs() {
    for (unsigned pin : {kTurnLeft, kTurnRight, kHeadlightL, kHeadlightR, kHorn}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, false);
    }
}

// the steady outputs; turn signals blink on their own timer
void drive(const Outputs& o) {
    gpio_put(kHeadlightL, o.headlights);
    gpio_put(kHeadlightR, o.headlights);
    gpio_put(kHorn, o.horn);
}

void all_off(Outputs& o) {
    o = Outputs{};
    drive(o);
    gpio_put(kTurnLeft, false);
    gpio_put(kTurnRight, false);
}

void apply_command(Outputs& o, const AuxCommand& c) {
    const bool on = c.value != 0;
    switch (c.actuator) {
        case Actuator::TurnLeft:
            o.turn_left = on;
            if (!on) {
                gpio_put(kTurnLeft, false);
            }
            break;
        case Actuator::TurnRight:
            o.turn_right = on;
            if (!on) {
                gpio_put(kTurnRight, false);
            }
            break;
        case Actuator::Headlights: o.headlights = on; break;
        case Actuator::Horn: o.horn = on; break;
        default: return;
    }
    drive(o);
}

}  // namespace

// front_aux node: turn/headlight/horn from commands, wheel speed and steering as telemetry
int main() {
    stdio_init_all();
    init_outputs();
    cev::rpm_init(kRpmLeft, kRpmRight);
    adc_init();
    adc_gpio_init(kSteeringAdc);

    cev::McpBus bus{{.sck  = kSpiSck,
                     .mosi = kSpiMosi,
                     .miso = kSpiMiso,
                     .cs   = kMcpCs,
                     .stby = kMcpStby,
                     .nint = kMcpInt}};

    Outputs out{};
    // a stop latches until reset; todo clear on a ratified resume command
    bool stopped{};
    bool blink_on{};
    cev::Interval blink{kBlinkHalfPeriodMs};
    cev::Interval pub{kPublishPeriodMs};
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
                all_off(out);
            } else if (!stopped && m.cls == chuds::MsgClass::Command && m.subaddress == kNodeId &&
                       m.type == chuds::MsgType::Update) {
                if (auto c = chuds::body_as<AuxCommand>(m)) {
                    apply_command(out, *c);
                    command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
                }
            }
        }

        if (time_reached(command_deadline)) {
            all_off(out);
            command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
        }

        if (blink.due()) {
            blink_on = !blink_on;
            gpio_put(kTurnLeft, out.turn_left && blink_on);
            gpio_put(kTurnRight, out.turn_right && blink_on);
        }

        if (pub.due()) {
            const cev::RpmCounts rpm = cev::rpm_take();
            adc_select_input(kSteeringChannel);
            bus.publish(kNodeId, Telemetry{rpm.left, rpm.right, adc_read()});
        }
    }
}
