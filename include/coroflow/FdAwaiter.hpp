#pragma once

#include <coroutine>
#include <system_error>

#include <coroflow/EventLoop.hpp>
#include <coroflow/Result.hpp>

namespace coroflow {

struct FdReadableAwaiter {
    EventLoop &loop;
    int fd;
    std::error_code pendingError{};

    bool await_ready() const noexcept {
        return false;
    }

    bool await_suspend(std::coroutine_handle<> handle) noexcept {
        auto result = loop.addReadEvent(fd, handle);
        if (!result) {
            pendingError = result.error();
            return false;
        }
        return true;
    }

    Result<void, std::error_code> await_resume() noexcept {
        if (pendingError)
            return Result<void, std::error_code>::failure(pendingError);
        return Result<void, std::error_code>::success();
    }
};

inline FdReadableAwaiter waitReadable(EventLoop& loop, int fd) noexcept {
    return FdReadableAwaiter{loop, fd};
}


}