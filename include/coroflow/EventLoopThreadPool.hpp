#pragma once

#include <coroflow/EventLoop.hpp>
#include <coroflow/Result.hpp>

#include <thread>
#include <vector>

namespace coroflow {

class EventLoopThreadPool {
public:
    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

    EventLoopThreadPool(EventLoopThreadPool &&other) noexcept = default;
    EventLoopThreadPool& operator=(EventLoopThreadPool &&other) noexcept = default;

    // 工厂函数
    static Result<EventLoopThreadPool, std::error_code> create(size_t threadNums) {
        if (threadNums == 0) {
            return Result<EventLoopThreadPool, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }

        // 先创建 EventLoop
        EventLoopThreadPool pool;
        pool.loops.reserve(threadNums);
        for (size_t i = 0; i < threadNums; ++i) {
            auto loopResult = EventLoop::create();
            if (!loopResult) {
                return Result<EventLoopThreadPool, std::error_code>::failure(
                    loopResult.error());
            }
            pool.loops.push_back(std::move(loopResult).value());
        }
        return Result<EventLoopThreadPool, std::error_code>::success(
            std::move(pool));
    }

    ~EventLoopThreadPool() {
        stop();
    }

    // 启动的时候，再添加 thread 开始运行
    Result<void, std::error_code> run() {
        if (running) { // 已经启动
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }

        try {
            for (size_t i = 0; i < loops.size(); ++i) {
                EventLoop* loop = &loops[i];
                threads.emplace_back([loop]() {
                    loop->run();
                });
            }
        } catch (const std::system_error& e) {
            for (auto& loop : loops)
                loop.stop();

            for (auto& thread : threads)
                if (thread.joinable())
                    thread.join();


            threads.clear();
            return Result<void, std::error_code>::failure(e.code());
        }

        running = true;
        return Result<void, std::error_code>::success();
    }

    // 停止逻辑，让loop先停下来，才能join
    void stop() {
        if (!running)
            return;

        for (auto& loop : loops)
            loop.stop();

        for (auto& thread : threads)
            if (thread.joinable())
                thread.join();

        threads.clear();
        running = false;
    }

    // 跨线程投递接口
    Result<void, std::error_code> pushConnfd(size_t index, int connFd) {
        if (index >= loops.size()) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }
        loops[index].pushConnfd(connFd);
        return Result<void, std::error_code>::success();
    }

    size_t size() const noexcept {
        return loops.size();
    }

private:
    EventLoopThreadPool() = default;

private:
    std::vector<EventLoop> loops;
    std::vector<std::thread> threads;
    bool running{false};
};

}