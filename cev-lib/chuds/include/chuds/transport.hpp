#pragma once

#include <concepts>
#include <cstdint>

#include "chuds/frame.hpp"

namespace chuds {

// outcome of a send; Queued means accepted into the TX queue, not yet on the wire
enum class TxStatus : std::uint8_t {
    Queued,
    QueueFull,  // transient, retry later
    BusOff,
    Error,
};

// outcome of a recv; the frame is valid only when status is Received
enum class RxStatus : std::uint8_t {
    Received,
    Empty,  // nothing pending, not an error
    Overflow,
    BusOff,
    Error,
};

struct RxResult {
    RxStatus status = RxStatus::Empty;
    CanFrame frame{};
};

// the seam between chuds and a driver, as a concept so the binding is
// compile-time with no vtable
// send and recv report explicit outcomes so a dead bus is not mistaken for quiet
template <typename T>
concept Transport = requires(T t, const CanFrame& f) {
    { t.send(f) } -> std::same_as<TxStatus>;
    { t.recv() } -> std::same_as<RxResult>;
};

}  // namespace chuds
