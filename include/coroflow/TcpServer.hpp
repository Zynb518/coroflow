#pragma once

#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>
#include <coroflow/IoLoopSelector.hpp>
#include <coroflow/EventLoopThreadPool.hpp>
#include <coroflow/SocketAddress.hpp>
#include <coroflow/Result.hpp>
#include <coroflow/TcpSession.hpp>

#include <system_error>

#include <functional>
#include <memory>
#include <coroflow/Task.hpp>
#include <coroflow/TcpConnection.hpp>
#include <coroflow/Logger.hpp>

namespace coroflow {

class TcpServer {
public:
    using SessionHandler = std::function<Task<void>(TcpSession)>;

    // 工厂函数
    static Result<TcpServer, std::error_code> create(const SocketAddress& listenAddress,
                                                     size_t threadNums) {
        auto accpetorResult = TcpAcceptor::create();
        if (!accpetorResult) {
            LOG_ERROR << "[TcpServer] 创建 acceptor 失败";
            return Result<TcpServer, std::error_code>::failure(
                accpetorResult.error());
        }

        auto loopResult = EventLoop::create();
        if (!loopResult) {
            LOG_ERROR << "[TcpServer] 创建 mainLoop 失败";
            return Result<TcpServer, std::error_code>::failure(
                loopResult.error());
        }

        auto poolResult = EventLoopThreadPool::create(threadNums);
        if (!poolResult) {
            LOG_ERROR << "[TcpServer] 创建 EventLoopPool 失败";
            return Result<TcpServer, std::error_code>::failure(
                poolResult.error());
        }

        auto listenResult = accpetorResult.value().listen(listenAddress);
        if (!listenResult) {
            LOG_ERROR << "[TcpServer] 监听地址失败";
            return Result<TcpServer, std::error_code>::failure(
                listenResult.error());
        }

        return Result<TcpServer, std::error_code>::success(
            TcpServer(std::move(accpetorResult).value(),
                std::move(loopResult).value(),
                threadNums,
                std::move(poolResult).value() ));
    }

    ~TcpServer() {
        stop();
    }

    TcpServer (const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer (TcpServer&&) = default;
    TcpServer& operator=(TcpServer&&) = default;

    void setConnectionHandler(SessionHandler handler) {
        sessionHandler_ = std::move(handler);
        workerThreadPool_.setSessionHandler(sessionHandler_);
    }


    Task<void> acceptLoop() {
        for (;;) {
            auto acceptResult = co_await acceptor_.accept(mainLoop_);
            if (!acceptResult) continue;
            int connFd = acceptResult.value();
            LOG_INFO << "接受到连接，文件描述符为:" << connFd;
            auto dispatchResult = dispatchConnFd(connFd);
            if (!dispatchResult) {
                LOG_ERROR << "分发连接文件描述符失败" << dispatchResult.error();
                ::close(connFd);
                continue;
            }

        }
    }

    Result<void, std::error_code> run() {
        if (running_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }

        if (!sessionHandler_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }

        auto poolRunResult = workerThreadPool_.run();
        if (!poolRunResult) {
            return Result<void, std::error_code>::failure(
                poolRunResult.error());
        }
        acceptTask_ = acceptLoop();
        acceptTask_.resume(); // 推进到第一次 co_await accept, 每次循环都添加进mainLoop

        running_ = true;
        mainLoop_.run(); // 主线程开始跑 accept loop
        acceptTask_ = Task<void>{}; // 这里再销毁协程帧才安全
        running_ = false;

        return Result<void, std::error_code>::success();
    }

    void stop() {
        if (!running_)
            return;

        mainLoop_.stop();
        workerThreadPool_.stop();
    }


private:
    Result<void, std::error_code> dispatchConnFd(int connFd) {
        size_t index = selector_.next();
        auto pushResult = workerThreadPool_.pushConnfd(index, connFd);
        if (!pushResult) {
            return Result<void, std::error_code>::failure(
                pushResult.error());
        }
        return Result<void, std::error_code>::success();
    }

private:
    explicit TcpServer(TcpAcceptor &&acceptor,
                       EventLoop &&mainLoop,
                       size_t threadNums,
                       EventLoopThreadPool &&pool)
        : acceptor_(std::move(acceptor)),
          mainLoop_(std::move(mainLoop)),
          selector_(threadNums),
          workerThreadPool_(std::move(pool)) {
    }
private:
    TcpAcceptor acceptor_;
    EventLoop mainLoop_;
    IoLoopSelector selector_;
    EventLoopThreadPool workerThreadPool_;

    SessionHandler sessionHandler_;
    Task<void> acceptTask_; // TcpServer 的长期后台任务，与TcpServer同生死

    bool running_{false};
};
}
