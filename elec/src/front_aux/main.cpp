#include <cstdint>
#include <initializer_list>

#include "chuds/io.hpp"
#include "common/mcp_bus.hpp"
#include "common/rpm.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "pico/stdlib.h"

// this node's id: telemetry source and command recipient
constexpr std::uint8_t kNodeId = 0x10;

// steering sensor adc channel, derived from its gpio
constexpr unsigned kSteeringChannel = kSteeringAdc - 26;

constexpr std::uint32_t kBlinkHalfPeriodMs = 333;
constexpr std::uint32_t kPublishPeriodMs   = 100;

namespace {

// telemetry body, published once per cycle
struct Telemetry {
    std::uint16_t rpm_left;   // wheel edges in the last window
    std::uint16_t rpm_right;  // wheel edges in the last window
    std::uint16_t steering;   // raw adc, 0-4095
};
static_assert(sizeof(Telemetry) == 6);

enum class Actuator : std::uint8_t {
    TurnLeft,
    TurnRight,
    Headlights,
    Horn,
};

// command body: set one actuator from the bus
struct AuxCommand {
    Actuator actuator;
    std::uint8_t on;
};
static_assert(sizeof(AuxCommand) == 2);

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
    const bool on = c.on != 0;
    switch (c.actuator) {
        case Actuator::TurnLeft: o.turn_left = on; break;
        case Actuator::TurnRight: o.turn_right = on; break;
        case Actuator::Headlights: o.headlights = on; break;
        case Actuator::Horn: o.horn = on; break;
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

    cev::McpBus bus{{kSpiSck, kSpiMosi, kSpiMiso, kMcpCs, kMcpStby}};
    auto& tx = bus.transport();

    Outputs out{};
    // a stop latches until reset; todo clear on a ratified resume command
    bool stopped{};
    bool blink_on{};
    absolute_time_t next_blink = make_timeout_time_ms(kBlinkHalfPeriodMs);
    absolute_time_t next_pub   = make_timeout_time_ms(kPublishPeriodMs);

    while (true) {
        if (auto rx = chuds::recv(tx); rx.status == chuds::RxStatus::Received && rx.msg) {
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
            adc_select_input(kSteeringChannel);
            const Telemetry t{rpm.left, rpm.right, adc_read()};
            if (auto m = chuds::make_message(chuds::MsgClass::Telemetry, kNodeId,
                                             chuds::MsgType::Update, t)) {
                chuds::send(tx, *m);
            }
        }
    }
}
