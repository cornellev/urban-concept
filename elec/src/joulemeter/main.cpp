#include <cstdint>
#include <cstdio>

#include "chuds/io.hpp"
#include "common/mcp_bus.hpp"
#include "config.hpp"
#include "hardware/adc.h"
#include "pico/stdlib.h"

// this node's id: telemetry source
constexpr std::uint8_t kNodeId = 0x30;

namespace {

// telemetry body, published every report period
struct Telemetry {
    float voltage;  // bus volts
    float current;  // bus amps
    float power;    // watts
    float energy;   // joules since boot
};
static_assert(sizeof(Telemetry) == 16);

// adc channel is the gpio offset from the adc base at gpio 26
unsigned channel_of(unsigned gpio) { return gpio - 26; }

// discard one sample after switching the mux per the datasheet
std::uint16_t read_raw(unsigned gpio) {
    adc_select_input(channel_of(gpio));
    (void)adc_read();
    return adc_read();
}

float raw_to_volts(std::uint16_t raw) { return static_cast<float>(raw) / kAdcCountsMax * kAdcVref; }

chuds::TxStatus publish(cev::Mcp251863Transport& tx, const Telemetry& t) {
    if (auto m =
            chuds::make_message(chuds::MsgClass::Telemetry, kNodeId, chuds::MsgType::Update, t)) {
        return chuds::send(tx, *m);
    }
    return chuds::TxStatus::Error;
}

}  // namespace

// joulemeter node: bus current and voltage, integrated to joules, published as telemetry
int main() {
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    adc_init();
    adc_gpio_init(kVoltageAdc);
    adc_gpio_init(kCurrentAdc);

    cev::McpBus bus{{kSpiSck, kSpiMosi, kSpiMiso, kMcpCs, kMcpStby}};
    auto& tx = bus.transport();

    // latched so a persistent fault prints once, not every loop
    bool bus_ok = bus.ok();

    constexpr std::uint32_t samples_per_report = kReportPeriodMs / kSamplePeriodMs;

    // energy accumulates over a whole run, so integrate in double to hold resolution
    double joules{};
    std::uint32_t n{};
    absolute_time_t prev = get_absolute_time();

    while (true) {
        const float v_bus = raw_to_volts(read_raw(kVoltageAdc)) * kVoltageDividerRatio;
        const float amps =
            (raw_to_volts(read_raw(kCurrentAdc)) - kCurrentZeroVolts) / kCurrentVoltsPerAmp;
        const float power = v_bus * amps;

        // integrate over the real elapsed interval, not the nominal period
        const absolute_time_t now = get_absolute_time();
        const double dt = static_cast<double>(absolute_time_diff_us(prev, now)) / 1'000'000.0;
        prev            = now;
        joules += static_cast<double>(power) * dt;

        if (++n >= samples_per_report) {
            n = 0;
            const chuds::TxStatus st =
                publish(tx, {v_bus, amps, power, static_cast<float>(joules)});
            if (bus_ok && (st == chuds::TxStatus::BusOff || st == chuds::TxStatus::Error)) {
                std::printf("can tx fault\n");
                bus_ok = false;
            }
            printf("v=%.2f V  i=%.2f A  p=%.1f W  e=%.1f J\n", v_bus, amps, power, joules);
        }

        sleep_ms(kSamplePeriodMs);
    }
}
