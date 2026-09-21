#pragma once

#include <optional>
#include <utility>

#include "chuds/message.hpp"
#include "chuds/transport.hpp"

namespace chuds {

// a received Message plus the transport status it arrived with
// mirrors RxResult one level up
// msg is set only when status is Received
struct RxMessage {
    RxStatus status{RxStatus::Empty};
    std::optional<Message> msg{};
};

// send a Message over any Transport
// encodes internally so callers never build a CanFrame
// Error if the message cannot be encoded
template <Transport T>
[[nodiscard]] TxStatus send(T& t, const Message& m) {
    const auto f = encode(m);
    if (!f) {
        return TxStatus::Error;
    }
    return t.send(*f);
}

// receive the next Message from any Transport, decoding internally
// status carries the bus state
// a frame that arrives but fails to decode is Malformed, not Received
template <Transport T>
[[nodiscard]] RxMessage recv(T& t) {
    const auto r = t.recv();
    if (r.status != RxStatus::Received) {
        return {r.status, std::nullopt};
    }
    auto m = decode(r.frame);
    if (!m) {
        return {RxStatus::Malformed, std::nullopt};
    }
    return {RxStatus::Received, std::move(m)};
}

}  // namespace chuds
