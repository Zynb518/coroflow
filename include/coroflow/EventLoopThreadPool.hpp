#pragma once

#include <coroflow/EventLoopThread.hpp>
#include <vector>

namespace coroflow {

class EventLoopThreadPool {
public:

private:
    std::vector<EventLoopThread> workerThreads;
};

}