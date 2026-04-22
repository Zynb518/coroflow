#pragma once

#include <coroflow/Epoller.hpp>
#include <coroflow/TcpConnection.hpp>
#include <coroflow/EventNotifier.hpp>
#include <coroflow/TimerQueue.hpp>

#include <vector>

namespace coroflow {

class EventLoop {
public:

private:
    Epoller epoller;
    EventNotifier eventNotifier;
    std::vector<TcpConnection> connections; // 管理的连接，暂时用vector
    TimerQueue timerQueue;
};

}
