#pragma once

#include <coroflow/TcpConnection.hpp>
#include <coroflow/EventLoop.hpp>

namespace coroflow {

inline bool TcpConnection::ReadAwaiter::await_ready() const noexcept {
    return false;
}

inline bool TcpConnection::ReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    auto result = loop.addReadEvent(conn.fd(), handle);
    if (!result) {
        pendingError = result.error();
        return false;
    }

    return true;
}

inline Result<size_t, std::error_code> TcpConnection::ReadAwaiter::await_resume() noexcept {
    if (pendingError) {
        return Result<size_t, std::error_code>::failure(pendingError);
    }

    return conn.readFromFd();
}

inline TcpConnection::ReadAwaiter TcpConnection::readable(EventLoop& loop) noexcept {
    return ReadAwaiter{*this, loop};
}

}
