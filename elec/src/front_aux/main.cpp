#include <cstdint>
#include <initializer_list>

#include "chuds/chuds.hpp"
#include "common/interval.hpp"
#include "common/mcp_bus.hpp"
#include "common/rpm.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "pico/stdlib.h"

using namespace chuds::front_aux;

// steering sensor adc channel, derived from its gpio
static_assert(kSteeringAdc >= 26 && kSteeringAdc <= 29);
constexpr unsigned kSteeringChannel = kSteeringAdc - 26;

constexpr std::uint32_t kPublishPeriodMs = 100;
// the leader must resend body state within this period or outputs fall back to their safe state
constexpr std::uint32_t kCommandTimeoutMs = 1000;
// frames handled per loop pass, the mcp rx fifo depth, so a flooded bus cannot starve the timers
constexpr int kMaxRxPerPass = 8;

namespace {

void init_outputs() {
    for (unsigned pin : {kTurnLeft, kTurnRight, kHeadlightL, kHeadlightR, kHorn}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, false);
    }
}

// drive this node's outputs from body state bits
void apply(std::uint8_t bits) {
    const bool lit        = (bits & chuds::BodyState::kBlinkPhase) != 0;
    const bool headlights = (bits & chuds::BodyState::kHeadlights) != 0;
    gpio_put(kTurnLeft, lit && (bits & chuds::BodyState::kLeftTurn) != 0);
    gpio_put(kTurnRight, lit && (bits & chuds::BodyState::kRightTurn) != 0);
    gpio_put(kHeadlightL, headlights);
    gpio_put(kHeadlightR, headlights);
    gpio_put(kHorn, (bits & chuds::BodyState::kHorn) != 0);
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

    // body state bits currently applied
    std::uint8_t body{};
    // a stop latches until reset; todo clear on a ratified resume command
    bool stopped{};
    cev::Interval pub{kPublishPeriodMs};
    absolute_time_t command_deadline = make_timeout_time_ms(kCommandTimeoutMs);

    while (true) {
        for (int i = 0; i < kMaxRxPerPass; ++i) {
            const auto rx = bus.recv();
            if (!rx) {
                break;
            }
            const chuds::Message& m = *rx;
            if (m.cls == chuds::MsgClass::Emergency && m.type == chuds::MsgType::Stop) {
                stopped = true;
                body &= chuds::BodyState::kHeadlights;
                apply(body);
            } else if (!stopped && m.cls == chuds::MsgClass::Command &&
                       m.subaddress == chuds::kBodyStateId && m.type == chuds::MsgType::Update) {
                if (auto s = chuds::body_as<chuds::BodyState>(m)) {
                    body = s->bits;
                    apply(body);
                    command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
                }
            }
        }

        // headlights hold, here and on a stop, since going dark is worse than staying lit
        if (time_reached(command_deadline)) {
            body &= chuds::BodyState::kHeadlights;
            apply(body);
            command_deadline = make_timeout_time_ms(kCommandTimeoutMs);
        }

        if (pub.due()) {
            const cev::RpmCounts rpm = cev::rpm_take();
            adc_select_input(kSteeringChannel);
            bus.publish(kNodeId, Telemetry{rpm.left, rpm.right, adc_read()});
        }
    }
}
