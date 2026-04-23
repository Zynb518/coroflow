#pragma once
#include <cerrno>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <sys/epoll.h>

#include "Result.hpp"

namespace coroflow {

class Epoller {
public:
    Epoller(const Epoller&) = delete;
    Epoller& operator=(const Epoller&) = delete;

    Epoller(Epoller && other) noexcept {
        epfd_ = std::exchange(other.epfd_, -1);
    }

    Epoller& operator=(Epoller && other) noexcept {
        if (this == &other)
            return *this;

        if (epfd_ != -1)
            ::close(epfd_);
        epfd_ = std::exchange(other.epfd_, -1);
        return *this;
    }

    // 工厂函数
    static Result<Epoller, std::error_code> create() {
        // 工程上常用
        int epfd = ::epoll_create1(EPOLL_CLOEXEC);
        if (epfd == -1) {
            return Result<Epoller, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<Epoller, std::error_code>::success(
            Epoller(epfd));
    }

    ~Epoller() {
        if (epfd_ != -1)
            ::close(epfd_);
    }

    // 增
    Result<void, std::error_code> add(int fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        int ret = ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);if (ret == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
        return Result<void, std::error_code>::success();
    }

    // 改
    Result<void, std::error_code> mod(int fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        int ret = ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
        if (ret == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
        return Result<void, std::error_code>::success();
    }
    // 删
    Result<void, std::error_code> del(int fd) {
        int ret = ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        if (ret == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category())
            );
        }

        return Result<void, std::error_code>::success();
    }

    // 等待事件
    Result<int, std::error_code> wait(epoll_event *epEvents,
                                      int maxEvents,
                                      int timeoutMs = -1) noexcept {
        if (epfd_ == -1) {
            return Result<int, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (epEvents == nullptr || maxEvents <= 0) {
            return Result<int, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }

        for (;;) {
            int readyCount = ::epoll_wait(epfd_, epEvents, maxEvents, timeoutMs);
            if (readyCount >= 0) {
                return Result<int, std::error_code>::success(readyCount);
            }

            if (errno == EINTR) continue;

            return Result<int, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
    }

private:
    explicit Epoller(int epfd)
        : epfd_(epfd) {}
private:
    int epfd_ {-1};
};

}
