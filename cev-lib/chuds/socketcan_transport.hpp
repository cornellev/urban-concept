#pragma once

#include "chuds/transport.hpp"  // IWYU pragma: keep

#ifdef __linux__

#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace cev {

// a chuds::Transport over a Linux SocketCAN raw CAN-FD socket
// non-blocking: recv reports Empty on a quiet bus, BusOff on a dead one
class SocketCanTransport {
   public:
    SocketCanTransport() = default;

    // disallow copy ops
    // allow move ops bc this transport owns the fd
    SocketCanTransport(const SocketCanTransport&)            = delete;
    SocketCanTransport& operator=(const SocketCanTransport&) = delete;

    SocketCanTransport(SocketCanTransport&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    SocketCanTransport& operator=(SocketCanTransport&& other) noexcept {
        if (this != &other) {
            close_fd();
            fd_       = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~SocketCanTransport() { close_fd(); }

    // open and bind a raw CAN-FD socket on ifname, e.g. "vcan0"
    // enables CAN-FD frames and non-blocking i/o
    // returns false on any failure
    [[nodiscard]] bool open(std::string_view ifname) {
        close_fd();

        int fd = ::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
        if (fd < 0) {
            return false;
        }

        int on = 1;
        if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &on, sizeof(on)) < 0) {
            ::close(fd);
            return false;
        }

        // deliver bus-off as an error frame so recv reports it instead of silence
        can_err_mask_t err_mask = CAN_ERR_BUSOFF;
        if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0) {
            ::close(fd);
            return false;
        }

        ifreq ifr{};
        // ifr is zero-initialized, so a clamped copy leaves ifr_name terminated
        const std::size_t cap = IFNAMSIZ - 1;
        const std::size_t n   = ifname.size() < cap ? ifname.size() : cap;
        std::memcpy(ifr.ifr_name, ifname.data(), n);
        if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
            ::close(fd);
            return false;
        }
        const int ifindex = ifr.ifr_ifindex;

        // the sockopt enables FD framing but does not prove the interface carries FD
        // an mtu below CANFD_MTU is classic-only
        if (::ioctl(fd, SIOCGIFMTU, &ifr) < 0 || ifr.ifr_mtu < static_cast<int>(CANFD_MTU)) {
            ::close(fd);
            return false;
        }

        sockaddr_can addr{};
        addr.can_family  = AF_CAN;
        addr.can_ifindex = ifindex;
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            return false;
        }

        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            ::close(fd);
            return false;
        }

        fd_ = fd;
        return true;
    }

    [[nodiscard]] bool is_open() const { return fd_ >= 0; }

    chuds::TxStatus send(const chuds::CanFrame& f) {
        if (fd_ < 0) {
            return chuds::TxStatus::Error;
        }

        // reject a malformed frame before it corrupts memory or aliases an id
        // an out-of-range id would mask down to a valid one on the wire
        if (!f.id.is_standard() || f.len > chuds::kMaxFdPayload) {
            return chuds::TxStatus::Error;
        }

        canfd_frame cf{};
        cf.can_id = f.id.raw();
        // frame contract is always CAN-FD with bit-rate switch
        cf.flags = CANFD_BRS;
        cf.len   = f.len;
        std::memcpy(cf.data, f.data.data(), f.len);

        ssize_t n = ::write(fd_, &cf, sizeof(cf));
        if (n == static_cast<ssize_t>(sizeof(cf))) {
            return chuds::TxStatus::Queued;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)) {
            return chuds::TxStatus::QueueFull;
        }
        return chuds::TxStatus::Error;
    }

    chuds::RxResult recv() {
        if (fd_ < 0) {
            return {chuds::RxStatus::Error, {}};
        }

        canfd_frame cf{};
        ssize_t n = ::read(fd_, &cf, sizeof(cf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return {chuds::RxStatus::Empty, {}};
            }
            return {chuds::RxStatus::Error, {}};
        }

        // an error frame reports bus state, not data
        // we registered only bus-off
        if ((cf.can_id & CAN_ERR_FLAG) != 0) {
            return {chuds::RxStatus::BusOff, {}};
        }

        // a 16-byte read is a classic CAN frame
        // chuds is CAN-FD only, so reject it
        if (n != static_cast<ssize_t>(sizeof(cf))) {
            return {chuds::RxStatus::Malformed, {}};
        }

        // chuds is standard-id CAN-FD only
        // reject remote and extended frames rather than forging a standard id
        if ((cf.can_id & (CAN_RTR_FLAG | CAN_EFF_FLAG)) != 0) {
            return {chuds::RxStatus::Malformed, {}};
        }
        // a standard frame carries no stray bits above the 11-bit id
        if ((cf.can_id & CAN_EFF_MASK) > CAN_SFF_MASK) {
            return {chuds::RxStatus::Malformed, {}};
        }

        chuds::CanFrame out{};
        out.id  = chuds::CanId::from_raw(static_cast<std::uint16_t>(cf.can_id & CAN_SFF_MASK));
        out.len = cf.len > chuds::kMaxFdPayload ? chuds::kMaxFdPayload : cf.len;
        std::memcpy(out.data.data(), cf.data, out.len);
        return {chuds::RxStatus::Received, out};
    }

   private:
    void close_fd() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd_ = -1;
};

static_assert(chuds::Transport<SocketCanTransport>);

}  // namespace cev

#endif  // __linux__
