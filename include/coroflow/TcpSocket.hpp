#pragma once

#include <system_error>
#include <utility>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

#include <coroflow/SocketAddress.hpp>
#include <coroflow/Result.hpp>

namespace coroflow {

class TcpSocket {
public:
    TcpSocket() = default;

    ~TcpSocket() {
        if (listenFd_ != -1)
            ::close(listenFd_);
    }

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 可移动，结合Result
    TcpSocket(TcpSocket&& other) noexcept
        :listenFd_(std::exchange(other.listenFd_, -1)) { }

    TcpSocket& operator=(TcpSocket&& other) noexcept {
        if (this == &other) return *this;
        if (listenFd_ != -1) ::close(listenFd_);

        listenFd_ = std::exchange(other.listenFd_, -1);
        return *this;
    }

    // 工厂函数
    static Result<TcpSocket, std::error_code> createTcpV4()  {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            return Result<TcpSocket, std::error_code>::failure(
                std::error_code(errno, std::generic_category() ));
        }
        return Result<TcpSocket, std::error_code>::success(TcpSocket(fd));
    }

    // 设置端口复用
    Result<void, std::error_code> setReuseAddr(bool enable = true) noexcept {
        if (listenFd_ == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        int optval = enable ? 1 : 0;
        if (::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<void, std::error_code>::success();
    }

    // 绑定
    Result<void, std::error_code> bind(const SocketAddress& address) noexcept {
        if (listenFd_ == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (::bind(listenFd_, address.sockaddrPtr(), address.sockaddrLen()) == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<void, std::error_code>::success();
    }

    // 监听
    Result<void, std::error_code> listen(int backlog) noexcept {
        if (listenFd_ == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (::listen(listenFd_, backlog) == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
        return Result<void, std::error_code>::success();
    }

    Result<int, std::error_code> accept() noexcept {
        if (listenFd_ == -1) {
            return Result<int, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        int connFd = ::accept4(listenFd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return Result<int, std::error_code>::failure(
                    std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            return Result<int, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<int, std::error_code>::success(connFd);
    }

    int fd() const noexcept {
        return listenFd_;
    }

private:
    explicit TcpSocket(int fd)
        :listenFd_(fd) { }

private:

    int listenFd_{-1};
};


}
