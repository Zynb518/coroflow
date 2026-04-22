#pragma once

#include <coroflow/Result.hpp>
#include <cstdint>
#include <system_error>
#include <cerrno>
#include <sys/eventfd.h>
#include <unistd.h>

namespace coroflow {

class EventNotifier {
public:
    EventNotifier(const EventNotifier&) = delete;
    EventNotifier& operator=(const EventNotifier&) = delete;

    EventNotifier(EventNotifier&&) noexcept = default;
    EventNotifier& operator=(EventNotifier&&) noexcept = default;

    static Result<EventNotifier, std::error_code> create() {
        int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd == -1) {
            return Result<EventNotifier, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
    }

    ~EventNotifier() {
        if (eventFd != -1)
            ::close(eventFd);
    }

    int fd() const noexcept {
        return eventFd;
    }

    // 除发可读
    Result<void, std::error_code> notify() noexcept {
        if (eventFd == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        uint64_t one = 1;
        ssize_t n = ::write(eventFd, &one, sizeof(one)); // 往 eventfd 写入一个 uint64_t
        if (n == -1 ) {
            if (errno == EAGAIN)  // 已经有未消费的唤醒信号，当前这次额外 notify 可以视为成功
                return Result<void, std::error_code>::success();


            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<void, std::error_code>::success();

    }

    // 把可读处理掉
    Result<void, std::error_code> handleRead() noexcept {
        if (eventFd == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        uint64_t value = 0;

        for (;;) {
            ssize_t n = ::read(eventFd, &value, sizeof(value));
            if (n == sizeof(value))
                return Result<void, std::error_code>::success();

            if (n == -1 && errno == EINTR) // 奇怪的中断 再尝试
                continue;

            if (n == -1 && errno == EAGAIN) // EAGAIN 表示在非阻塞模式下当前已经没有可读计数了
                return Result<void, std::error_code>::success();

            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category())
            );
        }

    }
private:
    explicit EventNotifier(int eventFd) noexcept
        : eventFd(eventFd) { }

private:
    int eventFd{-1};
};

}