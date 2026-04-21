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
        if (listen_fd != -1)
            ::close(listen_fd);
    }

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 可移动，结合Result
    TcpSocket(TcpSocket&& other) noexcept
        :listen_fd(std::exchange(other.listen_fd, -1)) { }

    TcpSocket& operator=(TcpSocket&& other) noexcept {
        if (this == &other) return *this;
        if (listen_fd != -1) ::close(listen_fd);

        listen_fd = std::exchange(other.listen_fd, -1);
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

    // 绑定
    Result<void, std::error_code> bind(const SocketAddress& address) noexcept {
        if (listen_fd == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (::bind(listen_fd, address.sockaddrPtr(), address.sockaddrLen()) == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        return Result<void, std::error_code>::success();
    }

    // 监听
    Result<void, std::error_code> listen(int backlog) noexcept {
        if (listen_fd == -1) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

        if (listen_fd == -1 || ::listen(listen_fd, backlog) == -1) {
            return Result<void, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }
        return Result<void, std::error_code>::success();
    }

    int fd() const noexcept {
        return listen_fd;
    }

private:
    explicit TcpSocket(int fd)
        :listen_fd(fd) { }

private:

    int listen_fd{-1};
};


}
