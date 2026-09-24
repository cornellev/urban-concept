#pragma once

#include <algorithm>
#include <cstdint>

#include "chuds/transport.hpp"
#include "mcp251863.h"

namespace cev {

// whether a driver timing preset runs at this bitrate and sample point on the 40 MHz clock
constexpr bool timing_matches(const BitTiming& t, std::uint32_t bitrate,
                              std::uint32_t sample_point) {
    const std::uint32_t tq_per_bit = std::uint32_t{t.tseg1} + t.tseg2 + 3;
    return MCP251863_CLOCK_HZ == bitrate * (std::uint32_t{t.brp} + 1) * tq_per_bit &&
           100 * (std::uint32_t{t.tseg1} + 2) == sample_point * tq_per_bit;
}

// McpBus applies these presets through init(), so they must match the chuds bus
static_assert(timing_matches(kBitTiming500K40MHz, chuds::kNominalBitrate,
                             chuds::kNominalSamplePoint));
static_assert(timing_matches(kBitTiming2M40MHz, chuds::kDataBitrate, chuds::kDataSamplePoint));

// a chuds::Transport over the MCP251863 CAN-FD controller
class Mcp251863Transport {
   public:
    // explicit constructor, taking in the controller by reference
    // because this is just a thin adapter
    explicit Mcp251863Transport(MCP251863& mcp) : mcp_(mcp) {}

    // disallow move and copy ops
    // copying the transport makes no sense, as it is bound to one controller
    Mcp251863Transport(const Mcp251863Transport&)            = delete;
    Mcp251863Transport& operator=(const Mcp251863Transport&) = delete;
    Mcp251863Transport(Mcp251863Transport&&)                 = delete;
    Mcp251863Transport& operator=(Mcp251863Transport&&)      = delete;
    ~Mcp251863Transport()                                    = default;

    [[nodiscard]] chuds::TxStatus send(const chuds::CanFrame& f) {
        if (!f.id.is_standard() || !chuds::is_valid_fd_len(f.len)) {
            return chuds::TxStatus::Error;
        }

        // chuds frames are CAN-FD with bit-rate switch and a standard id
        if (mcp_.send_canfd(f.id.raw(), f.data.data(), f.len, true, false) == 1) {
            return chuds::TxStatus::Queued;
        }

        // a bus-off keeps the tx queue full, so rule it out first
        if (mcp_.getStatus().bus_off) {
            return chuds::TxStatus::BusOff;
        }

        // the frame was prevalidated, so the only failure left is a full tx fifo
        return chuds::TxStatus::QueueFull;
    }

    [[nodiscard]] chuds::RxResult recv() {
        CanFdFrame cf = mcp_.read_canfd();
        if (!cf.valid) {
            // a dead bus is a fault, not a quiet one
            if (mcp_.getStatus().bus_off) {
                return {chuds::RxStatus::BusOff, {}};
            }

            if (mcp_.getFIFOStatus(mcp_.getRxFifoNum()).rx_overflow) {
                // the flag is sticky and holds nINT low, so clear it once reported
                mcp_.clearRxOverflow();
                return {chuds::RxStatus::Overflow, {}};
            }

            return {chuds::RxStatus::Empty, {}};
        }

        // chuds is standard 11-bit CAN-FD only
        // reject classic, remote, extended, out-of-range
        if (cf.ide || cf.rtr || !cf.fdf || cf.id > chuds::kMaxStandardId.raw()) {
            return {chuds::RxStatus::Malformed, {}};
        }

        chuds::CanFrame out{};
        out.id  = chuds::CanId::from_raw(static_cast<std::uint16_t>(cf.id));
        out.len = cf.len > chuds::kMaxFdPayload ? chuds::kMaxFdPayload : cf.len;
        std::copy_n(cf.data, out.len, out.data.data());

        return {chuds::RxStatus::Received, out};
    }

   private:
    MCP251863& mcp_;
};

// assert that this meets the requirement of a chuds::Transport
static_assert(chuds::Transport<Mcp251863Transport>);

}  // namespace cev
