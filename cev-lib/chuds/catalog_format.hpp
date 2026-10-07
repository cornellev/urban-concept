#pragma once

#include <cstdio>
#include <span>
#include <utility>

#include "chuds/catalog.hpp"
#include "chuds/message.hpp"

namespace chuds {

// write one readable line for m into out, truncated to fit
// returns the length snprintf would have written
inline int format_message(const Message& m, std::span<char> out) {
    if (const auto s = read_stop(m)) {
        return std::snprintf(out.data(), out.size(), "stop severity %d from 0x%02x", s->severity,
                             s->sender);
    }
    // boards act on a body state only when it is an Update, so print it only then
    if (m.cls == MsgClass::Command && m.subaddress == kBodyStateId && m.type == MsgType::Update) {
        if (const auto s = body_as<BodyState>(m)) {
            return std::snprintf(out.data(), out.size(), "body state 0x%02x", s->bits);
        }
    }
    if (m.cls == MsgClass::Telemetry && m.subaddress == front_aux::kNodeId) {
        if (const auto f = body_as<front_aux::Telemetry>(m)) {
            return std::snprintf(out.data(), out.size(), "front_aux rpm %d/%d steering %d",
                                 f->rpm_left, f->rpm_right, f->steering);
        }
    }
    if (m.cls == MsgClass::Telemetry && m.subaddress == back_aux::kNodeId) {
        if (const auto b = body_as<back_aux::Telemetry>(m)) {
            return std::snprintf(out.data(), out.size(), "back_aux rpm %d/%d brake %d", b->rpm_left,
                                 b->rpm_right, b->brake);
        }
    }
    if (m.cls == MsgClass::Telemetry && m.subaddress == joulemeter::kNodeId) {
        if (const auto j = body_as<joulemeter::Telemetry>(m)) {
            return std::snprintf(out.data(), out.size(), "joulemeter %.2f V %.2f A %.1f W %.1f J",
                                 j->voltage, j->current, j->power, j->energy);
        }
    }
    // not in the catalog, or its body size does not match
    return std::snprintf(
        out.data(), out.size(), "class %d subaddress 0x%02x type %d, %d body bytes",
        std::to_underlying(m.cls), m.subaddress, std::to_underlying(m.type), m.body_len);
}

// print one line for m to stdout
inline void print_message(const Message& m) {
    char line[96];
    format_message(m, line);
    std::printf("%s\n", line);
}

}  // namespace chuds
