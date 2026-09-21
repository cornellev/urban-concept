#pragma once

#include <concepts>
#include <cstdint>

#include "chuds/frame.hpp"

namespace chuds {

// outcome of a send
enum class TxStatus : std::uint8_t {
    // queued, not yet on the wire
    Queued,
    // tx queue full, retry later
    QueueFull,
    // controller is bus-off
    BusOff,
    // send failed, bad frame or driver fault
    Error,
};

// outcome of a recv
// the frame is valid only when status is Received
enum class RxStatus : std::uint8_t {
    // a valid frame arrived
    Received,
    // nothing pending, not a fault
    Empty,
    // rx queue overflowed, frames dropped
    Overflow,
    // non-chuds frame arrived, keep listening
    Malformed,
    // bus is off, a fault not silence
    BusOff,
    // recv failed, driver fault or closed transport
    Error,
};

struct RxResult {
    RxStatus status{RxStatus::Empty};
    CanFrame frame{};
};

// the seam between chuds and a driver
// send and recv report explicit outcomes so a dead bus is not mistaken for quiet
template <typename T>
concept Transport = requires(T t, const CanFrame& f) {
    { t.send(f) } -> std::same_as<TxStatus>;
    { t.recv() } -> std::same_as<RxResult>;
};

}  // namespace chuds
