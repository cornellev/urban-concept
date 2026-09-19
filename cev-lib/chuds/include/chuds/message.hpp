#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "chuds/frame.hpp"
#include "chuds/ids.hpp"

namespace chuds {

enum class MsgType : std::uint8_t {
    Update    = 0,
    Heartbeat = 1,
    Stop      = 2,
    Action    = 3,
    Dummy     = 4,
};

// payload is [type][body_len][body...]; the explicit length lets the receiver
// recover the true body size regardless of CAN-FD DLC padding
constexpr std::size_t kHeaderLen = 2;
constexpr std::size_t kMaxBody   = kMaxFdPayload - kHeaderLen;

// a decoded CHUDS message: the id fields (class + subaddress) and the typed body
struct Message {
    MsgClass cls     = MsgClass::Telemetry;
    std::uint8_t sub = 0;
    MsgType type     = MsgType::Dummy;
    std::array<std::uint8_t, kMaxBody> body{};
    std::uint8_t body_len = 0;

    // a view over the live body bytes, without the trailing capacity
    // clamps so a caller that set body_len past the buffer cannot read out of bounds
    [[nodiscard]] constexpr std::span<const std::uint8_t> body_view() const {
        return std::span(body).first(std::min<std::size_t>(body_len, body.size()));
    }
};

// build a message from a body view, or nullopt if the body is too long
[[nodiscard]] constexpr std::optional<Message> make_message(MsgClass cls, std::uint8_t sub,
                                                            MsgType type,
                                                            std::span<const std::uint8_t> body) {
    if (body.size() > kMaxBody) {
        return std::nullopt;
    }
    Message m{.cls = cls, .sub = sub, .type = type};
    std::copy_n(body.begin(), body.size(), m.body.begin());
    m.body_len = static_cast<std::uint8_t>(body.size());
    return m;
}

// STOP emergency class
// subaddress = severity (0 is most severe)
// body contains sender id and detail text
// detail can be up to kMaxBody - 1 bytes, since the first byte is reserved for the sender id
[[nodiscard]] constexpr std::optional<Message> make_stop(std::uint8_t severity, std::uint8_t sender,
                                                         std::string_view detail) {
    if (detail.size() + 1 > kMaxBody) {
        return std::nullopt;
    }
    Message m{.cls = MsgClass::Emergency, .sub = severity, .type = MsgType::Stop};
    m.body[0] = sender;
    std::copy_n(detail.begin(), detail.size(), m.body.begin() + 1);
    m.body_len = static_cast<std::uint8_t>(detail.size() + 1);
    return m;
}

// id = class + subaddress, payload = [type][body_len][body...]
[[nodiscard]] constexpr std::optional<CanFrame> encode(const Message& m) {
    if (m.body_len > kMaxBody) {
        return std::nullopt;
    }
    CanFrame f{};
    f.id      = make_id(m.cls, m.sub);
    f.data[0] = static_cast<std::uint8_t>(m.type);
    f.data[1] = m.body_len;
    std::copy_n(m.body.begin(), m.body_len, f.data.begin() + kHeaderLen);
    // round up to a valid DLC; the unused tail is already zero from init
    f.len = round_up_dlc(static_cast<std::uint8_t>(kHeaderLen + m.body_len));
    return f;
}

[[nodiscard]] constexpr std::optional<Message> decode(const CanFrame& f) {
    if (f.id > kMaxStandardId || f.len < kHeaderLen || f.len > kMaxFdPayload) {
        return std::nullopt;
    }
    // ids 0x300-0x7FF fall in the reserved class range with no defined MsgClass
    if (f.id > class_max(MsgClass::Telemetry)) {
        return std::nullopt;
    }
    const std::uint8_t type_byte = f.data[0];
    if (type_byte > static_cast<std::uint8_t>(MsgType::Dummy)) {
        return std::nullopt;
    }
    const std::uint8_t body_len = f.data[1];
    if (body_len > kMaxBody || kHeaderLen + body_len > f.len) {
        return std::nullopt;
    }
    Message m{};
    m.cls  = id_class(f.id);
    m.sub  = id_sub(f.id);
    m.type = static_cast<MsgType>(type_byte);
    std::copy_n(f.data.begin() + kHeaderLen, body_len, m.body.begin());
    m.body_len = body_len;
    return m;
}

}  // namespace chuds
