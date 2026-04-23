#pragma once

#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>
#include <coroflow/IoLoopSelector.hpp>
#include <coroflow/EventLoopThreadPool.hpp>
#include <coroflow/SocketAddress.hpp>
#include <coroflow/Result.hpp>

#include <system_error>

#include <functional>
#include <memory>
#include <coroflow/Task.hpp>
#include <coroflow/TcpConnection.hpp>


namespace coroflow {

namespace {

}

class TcpServer {
public:
    using ConnectionHandler =
        std::function<Task<void> (std::shared_ptr<TcpConnection>, EventLoop&) >;

    static Result<TcpServer, std::error_code> create(const SocketAddress& listenAddress,
                                                     size_t threadNums) {
        auto accpetorResult = TcpAcceptor::create();
        if (!accpetorResult) {
            return Result<TcpServer, std::error_code>::failure(
                accpetorResult.error());
        }

        auto loopResult = EventLoop::create();
        if (!loopResult) {
            return Result<TcpServer, std::error_code>::failure(
                loopResult.error());
        }
        auto listenResult = accpetorResult.value().listen(listenAddress);
        if (!listenResult) {
            return Result<TcpServer, std::error_code>::failure(
                listenResult.error());
        }


        // TODO: threadNums
        return Result<TcpServer, std::error_code>::success(
            TcpServer(std::move(accpetorResult).value(), std::move(loopResult).value(), threadNums ));
    }

    ~TcpServer() {

    }

    TcpServer (const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer (TcpServer&&) = default;
    TcpServer& operator=(TcpServer&&) = default;

    void setConnectionHandler(ConnectionHandler handler) {
        connectionHandler = std::move(handler);
    }

    Task<void> acceptLoop() {
        for (;;) {
            auto acceptResult = co_await acceptor.accept(mainLoop);
            if (!acceptResult) continue;

            int connFd = acceptResult.value();
            dispatchConnFd(connFd);
        }
    }

    void dispatchConnFd(int connFd) {
        size_t index = selector.next();
        EventLoop& ioLoop = workerThreadPool.getLoop(index);
        ioLoop.pushConnfd(connFd);
    }

    void run() {

    }

    void stop() {

    }

private:
    explicit TcpServer(TcpAcceptor&& acceptor, EventLoop&& mainLoop, size_t threadNums)
        :acceptor(std::move(acceptor)), mainLoop(std::move(mainLoop)), selector(threadNums) {

    }
private:
    TcpAcceptor acceptor;
    EventLoop mainLoop;
    IoLoopSelector selector;
    EventLoopThreadPool workerThreadPool;

    ConnectionHandler connectionHandler;
};
}
