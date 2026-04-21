#pragma once

#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>
#include <coroflow/IoLoopSelector.hpp>
#include <coroflow/EventLoopThreadPool.hpp>
#include <coroflow/SocketAddress.hpp>

#include <string>
#include <cstdint>
namespace coroflow {
class TcpServer {
public:
    explicit TcpServer(SocketAddress listenAddress, size_t threadNums )
        :acceptor() {

    }

    ~TcpServer() {

    }

    void run() {

    }

    TcpServer (const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer (TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

private:
    TcpAcceptor acceptor;
    EventLoop mainLoop;
    IoLoopSelector selector;
    EventLoopThreadPool workerThreadPool;
};
}
