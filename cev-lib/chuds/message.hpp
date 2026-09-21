#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

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

// a type value is defined only over Update..Dummy
[[nodiscard]] constexpr bool is_valid(MsgType type) { return type <= MsgType::Dummy; }

// payload is [type][body_len][body...]
// the explicit length recovers the true body size despite CAN-FD DLC padding
constexpr std::size_t kTypeOffset    = 0;
constexpr std::size_t kBodyLenOffset = 1;
constexpr std::size_t kHeaderLen     = 2;
constexpr std::size_t kMaxBody       = kMaxFdPayload - kHeaderLen;

// read the header fields out of a frame's data
[[nodiscard]] constexpr MsgType frame_type(const CanFrame& f) {
    return static_cast<MsgType>(f.data[kTypeOffset]);
}

[[nodiscard]] constexpr std::uint8_t frame_body_len(const CanFrame& f) {
    return f.data[kBodyLenOffset];
}

// a decoded CHUDS message: the id fields (class + subaddress) and the typed body
struct Message {
    MsgClass cls            = MsgClass::Telemetry;
    std::uint8_t subaddress = 0;
    MsgType type            = MsgType::Dummy;
    std::array<std::uint8_t, kMaxBody> body{};
    std::uint8_t body_len = 0;

    // a view over the body bytes, without the trailing capacity
    [[nodiscard]] constexpr std::span<const std::uint8_t> body_view() const {
        return std::span(body).first(std::min<std::size_t>(body_len, body.size()));
    }
};

// builds a message from a body view
// returns nullopt if the body is too long
[[nodiscard]] constexpr std::optional<Message> make_message(MsgClass cls, std::uint8_t sub,
                                                            MsgType type,
                                                            std::span<const std::uint8_t> body) {
    if (!is_valid(cls) || !is_valid(type) || body.size() > kMaxBody) {
        return std::nullopt;
    }

    Message m{.cls = cls, .subaddress = sub, .type = type};
    std::copy_n(body.begin(), body.size(), m.body.begin());
    m.body_len = static_cast<std::uint8_t>(body.size());

    return m;
}

// chuds struct bodies are the little-endian wire bytes. the host must match
static_assert(std::endian::native == std::endian::little);

// checks the requirements a struct body must meet to bit_cast onto the wire
template <class T>
constexpr void check_wire_body() {
    static_assert(std::is_trivially_copyable_v<T>, "body must be trivially copyable");
    static_assert(std::is_standard_layout_v<T>, "body must be standard-layout");
    static_assert(sizeof(T) <= kMaxBody, "body is larger than the CAN-FD payload");
}

// build a message whose body is a trivially-copyable struct
// the struct's bytes are the wire body, little-endian
template <class T>
    requires(!std::is_convertible_v<const T&, std::span<const std::uint8_t>>)
[[nodiscard]] constexpr std::optional<Message> make_message(MsgClass cls, std::uint8_t sub,
                                                            MsgType type, const T& value) {
    // ensure the struct can be sent over wire
    check_wire_body<T>();

    const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(value);

    return make_message(cls, sub, type, std::span<const std::uint8_t>(bytes));
}

// read the body of a message back as a struct
// returns nullopt if the body size is not exactly sizeof(T)
template <class T>
[[nodiscard]] constexpr std::optional<T> body_as(const Message& m) {
    // ensure the struct can be sent over wire
    check_wire_body<T>();

    if (m.body_len != sizeof(T)) {
        return std::nullopt;
    }

    std::array<std::uint8_t, sizeof(T)> bytes{};
    std::copy_n(m.body.begin(), sizeof(T), bytes.begin());

    return std::bit_cast<T>(bytes);
}

// make a STOP message
// transmits an emergency class with a subaddress as the severity (0 is the most severe)
// body contains [sender][detail...]
// overly long detail is truncated to prevent failures
// building a stop message should never fail, so we have a dedicated function for it
[[nodiscard]] constexpr Message make_stop(std::uint8_t severity, std::uint8_t sender,
                                          std::string_view detail) {
    constexpr std::size_t kMaxDetail = kMaxBody - 1;
    const std::size_t n              = detail.size() < kMaxDetail ? detail.size() : kMaxDetail;

    Message m{.cls = MsgClass::Emergency, .subaddress = severity, .type = MsgType::Stop};
    m.body[0] = sender;
    std::copy_n(detail.begin(), n, m.body.begin() + 1);
    m.body_len = static_cast<std::uint8_t>(n + 1);

    return m;
}

// a decoded STOP: severity from the subaddress, then [sender][detail...]
struct StopView {
    std::uint8_t severity = 0;
    std::uint8_t sender   = 0;
    std::span<const std::uint8_t> detail{};
};

// read a STOP, or nullopt if the message is not a well-formed STOP
[[nodiscard]] constexpr std::optional<StopView> read_stop(const Message& m) {
    if (m.cls != MsgClass::Emergency || m.type != MsgType::Stop || m.body_len < 1) {
        return std::nullopt;
    }

    StopView s{};
    s.severity = m.subaddress;
    s.sender   = m.body[0];
    s.detail   = m.body_view().subspan(1);

    return s;
}

// id = class + subaddress, payload = [type][body_len][body...]
[[nodiscard]] constexpr std::optional<CanFrame> encode(const Message& m) {
    // ensure validity of the class and type, and ensure body isn't too long
    if (!is_valid(m.cls) || !is_valid(m.type) || m.body_len > kMaxBody) {
        return std::nullopt;
    }

    CanFrame f{};
    f.id                   = CanId(m.cls, m.subaddress);
    f.data[kTypeOffset]    = static_cast<std::uint8_t>(m.type);
    f.data[kBodyLenOffset] = m.body_len;

    std::copy_n(m.body.begin(), m.body_len, f.data.begin() + kHeaderLen);

    // round up to a valid CAN-FD length
    // the unused tail is already zero from init
    f.len = round_up_fd_length(static_cast<std::uint8_t>(kHeaderLen + m.body_len));
    return f;
}

[[nodiscard]] constexpr std::optional<Message> decode(const CanFrame& f) {
    // ensure we are using a standard can-fd id, and id class is valid
    if (!f.id.is_standard() || !is_valid(f.id.cls())) {
        return std::nullopt;
    }

    // ensure the frame is longer than the header length, and is actually a valid length
    if (f.len < kHeaderLen || !is_valid_fd_len(f.len)) {
        return std::nullopt;
    }

    // ensure the frame type is valid
    if (!is_valid(frame_type(f))) {
        return std::nullopt;
    }

    const std::uint8_t body_len = frame_body_len(f);

    // the claimed body must fit within the frame
    if (body_len > kMaxBody || kHeaderLen + body_len > f.len) {
        return std::nullopt;
    }

    Message m{
        .cls        = f.id.cls(),
        .subaddress = f.id.subaddress(),
        .type       = frame_type(f),
    };

    std::copy_n(f.data.begin() + kHeaderLen, body_len, m.body.begin());
    m.body_len = body_len;

    return m;
}

}  // namespace chuds
