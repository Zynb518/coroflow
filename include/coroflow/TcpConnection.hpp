#pragma once

#include <coroflow/Result.hpp>
#include <coroflow/Buffer.hpp>

#include <coroutine>
#include <system_error>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>

namespace coroflow {

class EventLoop;


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

    State state() const noexcept {
        return state_;
    }

    size_t readableBytes() const noexcept {
        return readBuffer_.readable();
    }

    bool hasPendingWrite() const noexcept {
        return !writeBuffer_.empty();
    }

    std::string retrieve(size_t len) {
        return readBuffer_.retrieve(len);
    }

    std::string retrieveAll() {
        return readBuffer_.retrieveAll();
    }

    std::string_view peekReadable() const noexcept {
        return {readBuffer_.peek(), readBuffer_.readable()};
    }

    void appendWrite(std::string_view data) {
        writeBuffer_.append(data.data(), data.size());
    }


    // 接受客户端的消息
    Result<size_t, std::error_code> readFromFd() noexcept {
        if (fd_ == -1) {
            return Result<size_t, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        for (;;) {
            ssize_t n = readBuffer_.readFromFd(fd_);
            if (n > 0) {
                return Result<size_t, std::error_code>::success(
                    static_cast<size_t>(n));
            }

            if (n == 0) {
                state_ = State::Disconnected;
                return Result<size_t, std::error_code>::success(
                    static_cast<size_t>(0));
            }

            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return Result<size_t, std::error_code>::failure(
                    std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            return Result<size_t, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
    }

    // 给客户端发送消息
    Result<size_t, std::error_code> writeToFd() noexcept {
        if (fd_ == -1) {
            return Result<size_t, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (writeBuffer_.empty()) {
            return Result<size_t, std::error_code>::success(
                static_cast<size_t>(0));
        }

        for (;;) {
            ssize_t n = writeBuffer_.writeToFd(fd_);
            if (n >= 0) {
                return Result<size_t, std::error_code>::success(
                    static_cast<size_t>(n));
            }

            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return Result<size_t, std::error_code>::failure(
                    std::make_error_code(std::errc::resource_unavailable_try_again));
            }
            return Result<size_t, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
    }

    void shutdownWrite() noexcept {
        if (fd_ == -1)
            return;

        if (state_ != State::Connected)
            return;

        if (::shutdown(fd_, SHUT_WR) == 0)
            state_ = State::Disconnecting;
    }

    void close() noexcept {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
        state_ = State::Disconnected;
    }

private:
    int fd_{-1};
    State state_{State::Connected};
    Buffer readBuffer_;
    Buffer writeBuffer_;
};

}