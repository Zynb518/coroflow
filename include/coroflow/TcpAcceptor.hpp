#pragma once

#include <coroflow/TcpSocket.hpp>
#include <coroflow/SocketAddress.hpp>
#include <coroflow/Result.hpp>
#include <coroflow/EventLoop.hpp>

#include <system_error>
#include <coroutine>

namespace coroflow {

class TcpAcceptor {
public:
    // awaiter
    struct AcceptAwaiter {
        TcpSocket& socket;
        EventLoop& mainLoop;
        std::error_code pendingError{};

        bool await_ready() const noexcept{
            return false;
        }

        // true 表示挂起， false 表示不要挂起
        bool await_suspend(std::coroutine_handle<> handle) noexcept {
            auto result = mainLoop.addReadEvent(socket.fd(), handle);
            if (!result) {
                pendingError = result.error();
                return false;
            }
            return true;
        }

        Result<int, std::error_code> await_resume() noexcept {
            if (pendingError)
                return Result<int, std::error_code>::failure(pendingError);

            return socket.accept();

        }

    };

    ~TcpAcceptor() = default;

    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    // 可移动，结合Result
    TcpAcceptor(TcpAcceptor&&) noexcept = default;
    TcpAcceptor& operator=(TcpAcceptor&&) noexcept = default;

    // 工厂函数
    static Result<TcpAcceptor, std::error_code> create() {
        auto socketResult = TcpSocket::createTcpV4();
        if (!socketResult) {
            return Result<TcpAcceptor, std::error_code>::failure(
                std::move(socketResult).error());
        }
        // 相比 std::move(result.value()) 接口语义更清晰
        return Result<TcpAcceptor, std::error_code>::success(
            TcpAcceptor(std::move(socketResult).value()));
    }


    Result<void, std::error_code> listen(const SocketAddress &address,
                                         int backlog = SOMAXCONN) {
        auto reuseResult = socket.setReuseAddr(true);
        if (!reuseResult) {
            return Result<void, std::error_code>::failure(
                std::move(reuseResult).error());
        }

        auto bindResult = socket.bind(address);
        if (!bindResult) {
            return Result<void, std::error_code>::failure(
                std::move(bindResult).error());
        }
        return socket.listen(backlog);
    }

    // 协程风格 返回一个可等待的对象
    AcceptAwaiter accept(EventLoop& mainLoop) noexcept {
        return AcceptAwaiter{socket, mainLoop};
    }
private:
    explicit TcpAcceptor(TcpSocket && socket)
        :socket(std::move(socket)) {}

private:
    TcpSocket socket;
};

}