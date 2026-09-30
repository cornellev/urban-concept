#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

#include "chuds/chuds.hpp"
#include "chuds/socketcan_transport.hpp"

namespace {

// how often the body state is resent, well inside the boards' 1 s timeout
constexpr auto kBodyResend = std::chrono::milliseconds{300};
// half a turn-signal blink period
constexpr auto kBlinkHalf = std::chrono::milliseconds{333};

volatile std::sig_atomic_t g_stop = 0;

void on_sigint(int /*sig*/) { g_stop = 1; }

// the key pressed since the last call, or 0
char read_key() {
    pollfd p{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    char c = 0;
    if (::poll(&p, 1, 0) > 0 && ::read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return 0;
}

void print_state(std::uint8_t bits) {
    using chuds::BodyState;
    std::printf(
        "[body] headlights %s  left %s  right %s  horn %s  wiper %s\n",
        (bits & BodyState::kHeadlights) ? "on" : "off",
        (bits & BodyState::kLeftTurn) ? "on" : "off", (bits & BodyState::kRightTurn) ? "on" : "off",
        (bits & BodyState::kHorn) ? "on" : "off", (bits & BodyState::kWiper) ? "on" : "off");
}

// apply one key to the body bits, false if the key does nothing
bool apply_key(char key, std::uint8_t& bits) {
    using chuds::BodyState;
    // hazards are both turn signals
    constexpr std::uint8_t both = BodyState::kLeftTurn | BodyState::kRightTurn;
    switch (key) {
        case 'h': bits ^= BodyState::kHeadlights; return true;
        case 'l': bits ^= BodyState::kLeftTurn; return true;
        case 'r': bits ^= BodyState::kRightTurn; return true;
        case 'z':
            bits = static_cast<std::uint8_t>((bits & both) == both ? bits & ~both : bits | both);
            return true;
        case 'n': bits ^= BodyState::kHorn; return true;
        case 'w': bits ^= BodyState::kWiper; return true;
        default: return false;
    }
}

// print telemetry and drive the body outputs from the keyboard until q or ctrl-c
int run_live(chuds::Bus<chuds::SocketCanTransport>& bus) {
    using chuds::BodyState;
    using Clock = std::chrono::steady_clock;

    // read keys one at a time without echo, and put the terminal back on exit
    termios saved{};
    const bool tty = ::tcgetattr(STDIN_FILENO, &saved) == 0;
    if (tty) {
        termios raw = saved;
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
        ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    std::signal(SIGINT, on_sigint);

    std::printf("keys: h=headlights, l=left, r=right, z=hazards, n=horn, w=wiper, q=quit\n");

    std::uint8_t bits{};
    bool blink_on{};
    auto next_send  = Clock::now();
    auto next_blink = Clock::now() + kBlinkHalf;

    while (g_stop == 0) {
        const char key = read_key();
        if (key == 'q') {
            break;
        }
        const auto now = Clock::now();
        if (apply_key(key, bits)) {
            print_state(bits);
            next_send = now;
        }
        if (now >= next_blink) {
            blink_on = !blink_on;
            next_blink += kBlinkHalf;
            next_send = now;
        }
        if (now >= next_send) {
            const auto out =
                static_cast<std::uint8_t>(blink_on ? bits | BodyState::kBlinkPhase : bits);
            const BodyState state{out};
            (void)bus.send(chuds::MsgClass::Command, chuds::kBodyStateId, chuds::MsgType::Update,
                           state);
            next_send = now + kBodyResend;
        }

        if (!bus.wait(std::chrono::milliseconds{20})) {
            continue;
        }
        for (auto r = bus.recv(); r || r.error() != chuds::RxError::Empty; r = bus.recv()) {
            if (r) {
                chuds::print_message(*r);
            } else if (chuds::is_fault(r.error())) {
                std::fprintf(stderr, "recv fault: %d\n", std::to_underlying(r.error()));
                break;
            }
        }
    }

    if (tty) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    }
    return 0;
}

// send a body state with headlights on
int run_send(chuds::Bus<chuds::SocketCanTransport>& bus) {
    const chuds::BodyState state{chuds::BodyState::kHeadlights};
    const auto st =
        bus.send(chuds::MsgClass::Command, chuds::kBodyStateId, chuds::MsgType::Update, state);
    if (!st) {
        std::fprintf(stderr, "send failed: %d\n", std::to_underlying(st.error()));
        return 1;
    }
    std::printf("sent body state 0x%02x\n", state.bits);
    return 0;
}

// print the first message that arrives
int run_recv(chuds::Bus<chuds::SocketCanTransport>& bus, std::string_view ifname) {
    std::printf("listening on %.*s\n", static_cast<int>(ifname.size()), ifname.data());
    while (bus.wait(std::chrono::seconds{5})) {
        const auto r = bus.recv();
        if (r) {
            chuds::print_message(*r);
            return 0;
        }
        if (chuds::is_fault(r.error())) {
            std::fprintf(stderr, "recv failed: %d\n", std::to_underlying(r.error()));
            return 1;
        }
    }
    std::fprintf(stderr, "timeout, nothing received\n");
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view mode   = argc > 1 ? argv[1] : "";
    const std::string_view ifname = argc > 2 ? argv[2] : "vcan0";

    if (mode != "send" && mode != "recv" && mode != "run") {
        std::fprintf(stderr, "usage: %s send|recv|run [ifname]\n", argv[0]);
        return 2;
    }

    chuds::Bus<chuds::SocketCanTransport> bus;
    if (!bus.open(ifname)) {
        std::fprintf(stderr, "failed to open %.*s\n", static_cast<int>(ifname.size()),
                     ifname.data());
        return 1;
    }

    if (mode == "run") {
        return run_live(bus);
    }

    return mode == "send" ? run_send(bus) : run_recv(bus, ifname);
}
