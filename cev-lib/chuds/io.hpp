#pragma once

#include <expected>
#include <utility>

#include "chuds/message.hpp"
#include "chuds/transport.hpp"

namespace chuds {

// mirrors RxResult one level up
using RecvResult = std::expected<Message, RxError>;

// the RxError that reports a decode failure
// no default case, so -Wswitch flags a DecodeError added without a mapping
[[nodiscard]] constexpr RxError to_rx_error(DecodeError e) {
    switch (e) {
        case DecodeError::NonStandardId: return RxError::NonStandardId;
        case DecodeError::UnknownClass: return RxError::UnknownClass;
        case DecodeError::BadLength: return RxError::BadLength;
        case DecodeError::UnknownType: return RxError::UnknownType;
        case DecodeError::BodyOverrun: return RxError::BodyOverrun;
    }
    return RxError::Error;
}

// send a Message over any Transport
// encodes internally so callers never build a CanFrame
// Error if the message cannot be encoded
template <Transport T>
[[nodiscard]] TxResult send(T& t, const Message& m) {
    const auto f = encode(m);
    if (!f) {
        return std::unexpected(TxError::Error);
    }
    return t.send(*f);
}

// receive the next Message from any Transport, decoding internally
// a frame that arrives but fails to decode reports why as its RxError
template <Transport T>
[[nodiscard]] RecvResult recv(T& t) {
    const auto r = t.recv();
    if (!r) {
        return std::unexpected(r.error());
    }
    auto m = decode(*r);
    if (!m) {
        return std::unexpected(to_rx_error(m.error()));
    }
    return *std::move(m);
}

}  // namespace chuds
