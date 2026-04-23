#pragma once

#include <coroflow/Result.hpp>
#include <coroflow/Buffer.hpp>

#include <system_error>
#include <unistd.h>
namespace coroflow {
// 不需要工厂函数，因为是由外部注入构造的
class TcpConnection {
public:
    enum class State {
        Connected,
        Disconnecting,
        Disconnected,
    };

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    TcpConnection(TcpConnection&&) = delete;
    TcpConnection& operator=(TcpConnection&&) = delete;

    explicit TcpConnection(int fd) noexcept
        :fd_(fd) {}

    ~TcpConnection() {
        if (fd_ != -1)
            ::close(fd_);
    }

    int fd() const noexcept {
        return fd_;
    }

    Buffer& readBuffer() noexcept {
        return readBuffer_;
    }

    const Buffer& readBuffer() const noexcept {
        return readBuffer_;
    }

    Buffer& writeBuffer() noexcept {
        return writeBuffer_;
    }

    const Buffer& writeBuffer() const noexcept {
        return writeBuffer_;
    }


    State state() const noexcept {
        return state_;
    }

    Result<size_t, std::error_code> readFromFd() noexcept;

    Result<size_t, std::error_code> writeToFd() noexcept;

    void shutdownWrite() noexcept;

    void close() noexcept;


private:
    int fd_{-1};
    State state_{State::Connected};
    Buffer readBuffer_;
    Buffer writeBuffer_;
};

}