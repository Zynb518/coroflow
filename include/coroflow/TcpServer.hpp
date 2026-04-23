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
#include <coroflow/Logger.hpp>

namespace coroflow {

class TcpServer {
public:
    using ConnectionHandler =
        std::function<Task<void> (std::shared_ptr<TcpConnection>, EventLoop&) >;

    // 工厂函数
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

        return Result<TcpServer, std::error_code>::success(
            TcpServer(std::move(accpetorResult).value(), std::move(loopResult).value(), threadNums ));
    }

    ~TcpServer() {
        stop();
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
            auto dispatchResult = dispatchConnFd(connFd);
            if (!dispatchResult) {
                LOG_ERROR << "分发连接文件描述符失败";
                ::close(connFd);
                continue;
            }
        }
    }



    Result<void, std::error_code> run() {
        auto poolRunResult = workerThreadPool.run();
        if (!poolRunResult) {
            return Result<void, std::error_code>::failure(
                poolRunResult.error());
        }

        acceptTask = acceptLoop();
        acceptTask.resume(); // 推进到第一次 co_await accept

        mainLoop.run(); // 主线程开始跑 accept loop

        return Result<void, std::error_code>::success();
    }

    void stop() {
        mainLoop.stop();
        workerThreadPool.stop();
    }
private:
    Result<void, std::error_code> dispatchConnFd(int connFd) {
        size_t index = selector.next();
        auto pushResult = workerThreadPool.pushConnfd(index, connFd);
        if (!pushResult) {
            return Result<void, std::error_code>::failure(
                pushResult.error());
        }
        return Result<void, std::error_code>::success();
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
    Task<void> acceptTask; // TcpServer 的长期后台任务，与TcpServer同生死
};
}
