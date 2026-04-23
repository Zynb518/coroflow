#pragma once

#include <coroflow/EventLoop.hpp>
#include <coroflow/Result.hpp>

#include <thread>
#include <vector>

namespace coroflow {

class EventLoopThreadPool {
public:
    using EventLoopThread = std::pair<std::thread, EventLoop>;
    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

    EventLoopThreadPool(EventLoopThreadPool &&other)
        :workers(std::move(other.workers)){

    }

    EventLoopThreadPool& operator=(EventLoopThreadPool &&) {

    }

    static Result<EventLoopThreadPool, std::error_code> create() {

    }

    ~EventLoopThreadPool() {

    }

    void run() {}

    void pushConnfd(size_t index, int connFd) {

    }

    void stop() {

    }


private:
    std::vector<EventLoopThread> workers;
    std::atomic<bool> running{false};
};

}