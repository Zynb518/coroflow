#pragma once

#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>
#include <coroflow/IoLoopSelector.hpp>
#include <coroflow/EventLoopThreadPool.hpp>

namespace coroflow {
class TcpServer {
public:

private:
    TcpAcceptor acceptor;
    EventLoop mainLoop;
    IoLoopSelector selector;
    EventLoopThreadPool workerThreadPool;
};
}
