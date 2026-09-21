#pragma once

#include <cstring>

#include "chuds/transport.hpp"
#include "mcp251863.h"

namespace cev {

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

    chuds::TxStatus send(const chuds::CanFrame& f) {
        if (!f.id.is_standard() || !chuds::is_valid_fd_len(f.len)) {
            return chuds::TxStatus::Error;
        }

        // chuds frames are CAN-FD with bit-rate switch and a standard id
        if (mcp_.send_canfd(f.id.raw(), f.data.data(), f.len, true, false) == 1) {
            return chuds::TxStatus::Queued;
        }

        // a bus-off keeps the tx queue full, so rule it out before reading fullness
        if (mcp_.getStatus().bus_off) {
            return chuds::TxStatus::BusOff;
        }

        // not_full_or_not_empty is the tx not-full flag
        // clear means the queue is full
        if (!mcp_.getFIFOStatus(mcp_.getTxFifoNum()).not_full_or_not_empty) {
            return chuds::TxStatus::QueueFull;
        }

        return chuds::TxStatus::Error;
    }

    chuds::RxResult recv() {
        CanFdFrame cf = mcp_.read_canfd();
        if (!cf.valid) {
            if (mcp_.getFIFOStatus(mcp_.getRxFifoNum()).rx_overflow) {
                return {chuds::RxStatus::Overflow, {}};
            }

            // a dead bus is a fault, not a quiet one
            if (mcp_.getStatus().bus_off) {
                return {chuds::RxStatus::BusOff, {}};
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
        std::memcpy(out.data.data(), cf.data, out.len);

        return {chuds::RxStatus::Received, out};
    }

   private:
    MCP251863& mcp_;
};

// assert that this meets the requirement of a chuds::Transport
static_assert(chuds::Transport<Mcp251863Transport>);

}  // namespace cev
