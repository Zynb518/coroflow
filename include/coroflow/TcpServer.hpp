#pragma once

#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>
#include <coroflow/IoLoopSelector.hpp>
#include <coroflow/EventLoopThreadPool.hpp>
#include <coroflow/SocketAddress.hpp>
#include <coroflow/Result.hpp>

#include <system_error>


namespace coroflow {

namespace {

}

class TcpServer {
public:
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

};
}
