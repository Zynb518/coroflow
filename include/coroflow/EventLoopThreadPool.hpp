#pragma once

#include <coroflow/EventLoop.hpp>
#include <coroflow/Result.hpp>
#include <coroflow/TcpSession.hpp>
#include <coroflow/Task.hpp>

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

namespace coroflow {

class EventLoopThreadPool {
public:
    using SessionHandler = std::function<Task<void>(TcpSession)>;

    struct WorkerContext {
        explicit WorkerContext(EventLoop&& loop) noexcept
            : loop_(std::move(loop)) {}

        WorkerContext(const WorkerContext&) = delete;
        WorkerContext& operator=(const WorkerContext&) = delete;

        WorkerContext(WorkerContext&&) noexcept = default;
        WorkerContext& operator=(WorkerContext&&) noexcept = default;

        EventLoop loop_;
        std::thread thread_;
        std::vector<Task<void>> sessionTasks_;
    };

    // -----------------------------------------------------------------------------
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
        pool.workers_.reserve(threadNums);
        for (size_t i = 0; i < threadNums; ++i) {
            auto loopResult = EventLoop::create();
            if (!loopResult) {
                return Result<EventLoopThreadPool, std::error_code>::failure(
                    loopResult.error());
            }
            pool.workers_.emplace_back(std::move(loopResult).value()); // 在内部构造
        }
        return Result<EventLoopThreadPool, std::error_code>::success(
            std::move(pool));
    }

    ~EventLoopThreadPool() {
        stop();
    }

    // 设置会话处理协程
    void setSessionHandler(SessionHandler handler) {
        sessionHandler_ = std::move(handler);
    }

    // 启动的时候，再添加 thread 开始运行
    Result<void, std::error_code> run() {
        // 已经启动不得再启动一次
        if (running_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }
        // 没有设置处理函数 不能启动
        if (!sessionHandler_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }

        try {
            for (size_t i = 0; i < workers_.size(); ++i) {
                // 设置 callback
                auto& worker = workers_[i];
                worker.loop_.setNewConnectionCallback(
                    [this, i](std::shared_ptr<TcpConnection> conn, EventLoop& loop) {
                        if (!sessionHandler_)
                            return;

                        auto task = sessionHandler_(TcpSession(std::move(conn), loop));
                        if (!task.valid())
                            return;

                        task.resume();
                        workers_[i].sessionTasks_.push_back(std::move(task));
                        reapCompletedTasks(workers_[i].sessionTasks_);
                    });

                // 启动线程
                EventLoop* loop = &worker.loop_;
                worker.thread_ = std::thread([loop]() {
                    loop->run();
                });

            }
        } catch (const std::system_error& e) {
            for (auto& worker : workers_)
                worker.loop_.stop();

            for (auto& worker : workers_)
                if (worker.thread_.joinable())
                    worker.thread_.join();

            for (auto& worker : workers_)
                worker.sessionTasks_.clear();

            return Result<void, std::error_code>::failure(e.code());
        }

        running_ = true;
        return Result<void, std::error_code>::success();
    }

    // 停止逻辑，让loop先停下来，才能join，最后才清理sessionTasks_
    void stop() {
        if (!running_)
            return;

        for (auto& worker : workers_)
            worker.loop_.stop();

        for (auto& worker : workers_)
            if (worker.thread_.joinable())
                worker.thread_.join();

        for (auto& worker : workers_)
            worker.sessionTasks_.clear();

        running_ = false;
    }


    // 跨线程投递接口
    Result<void, std::error_code> pushConnfd(size_t index, int connFd) {
        if (index >= workers_.size()) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }
        workers_[index].loop_.pushConnfd(connFd);
        return Result<void, std::error_code>::success();
    }

    size_t size() const noexcept {
        return workers_.size();
    }

private:
    // 消除已完成的任务
    static void reapCompletedTasks(std::vector<Task<void>>& tasks) {
        tasks.erase(
            std::remove_if(tasks.begin(),
                           tasks.end(),
                           [](const Task<void>& task) {
                               return task.done();
                           }),
            tasks.end());
    }


    EventLoopThreadPool() = default;

private:
    std::vector<WorkerContext> workers_;
    SessionHandler sessionHandler_;
    bool running_{false};
};

}