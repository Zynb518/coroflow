#pragma once

#include <coroflow/Epoller.hpp>
#include <coroflow/TcpConnection.hpp>
#include <coroflow/EventNotifier.hpp>
#include <coroflow/TimerQueue.hpp>
#include <coroflow/Result.hpp>

#include <array>
#include <coroutine>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <memory>
#include <mutex>
#include <deque>

#include "FdContext.hpp"

namespace coroflow {

class EventLoop {
public:
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop &&other) noexcept
        : epoller_(std::move(other.epoller_)),
          notifier_(std::move(other.notifier_)),
          timerQueue_(std::move(other.timerQueue_)),
          fdContexts_(std::move(other.fdContexts_)),
          connections_(std::move(other.connections_)),
          running_(other.running_) {
    }

    EventLoop &operator=(EventLoop &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        epoller_ = std::move(other.epoller_);
        notifier_ = std::move(other.notifier_);
        timerQueue_ = std::move(other.timerQueue_);
        fdContexts_ = std::move(other.fdContexts_);
        connections_ = std::move(other.connections_);
        running_ = other.running_;

        return *this;
    }


    // 工厂函数
    static Result<EventLoop, std::error_code> create() {
        auto epollerResult = Epoller::create();
        if (!epollerResult) {
            return Result<EventLoop, std::error_code>::failure(
                epollerResult.error());
        }

        auto notifierResult = EventNotifier::create();
        if (!notifierResult) {
            return Result<EventLoop, std::error_code>::failure(
                notifierResult.error());
        }

        // 需要在内部注册好 notifier
        EventLoop loop(std::move(epollerResult).value(),
                       std::move(notifierResult).value());

        auto addResult = loop.epoller_.add(loop.notifier_.fd(), EPOLLIN | EPOLLET);
        if (!addResult) {
            return Result<EventLoop, std::error_code>::failure(
                addResult.error());
        }

        return Result<EventLoop, std::error_code>::success(std::move(loop));
    }

    ~EventLoop() = default;

    // 跑一次，就注销 handle， 但是 callback 不需要
    void run() {
        std::array<epoll_event, 64> epEvents{};
        running_ = true;

        while (running_) {
            auto waitResult = epoller_.wait(epEvents.data(),
                                           epEvents.size(), -1); // 阻塞 wait
            if (!waitResult)
                continue;
            int readyCount = waitResult.value();
            // 有事件
            for (int i = 0; i < readyCount; ++i) {
                int fd = epEvents[i].data.fd;
                // 被唤醒，单独处理
                if (fd == notifier_.fd()) {
                    notifier_.handleRead();
                    continue;
                }

                uint32_t revents = epEvents[i].events;

                auto it = fdContexts_.find(fd);
                if (it == fdContexts_.end())
                    continue;

                auto& ctx = it->second;
                if (revents & EPOLLIN) {
                    if (ctx.read_.handle_) {
                        auto handle = std::exchange(ctx.read_.handle_, std::coroutine_handle<>{});
                        handle.resume();
                    } else if (ctx.read_.callback_) {
                        ctx.read_.callback_();
                    }
                }
            } // for

            // 处理 fd 添加连接 , 内部加锁
            handlePendingConnFds();

        } // while
    }

    void stop() noexcept {
        running_ = false;
        notifier_.notify();
    }

    // “epoll 永久订阅、协程一次性消费”
    Result<void, std::error_code> addReadEvent(int fd,
                                               std::coroutine_handle<> handle) {
        auto& ctx = fdContexts_[fd];
        ctx.fd_ = fd;

        // 已经有句柄或者回调 不能重复注册
        if (ctx.read_.handle_ || ctx.read_.callback_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }
        ctx.read_.handle_ = handle;

        uint32_t newEvents = ctx.interestedEvents_ | EPOLLIN | EPOLLET; // 新的感兴趣的事件
        auto result = Result<void, std::error_code>::success();
        // 有interestedEvents = 已经注册epoll
        if ((ctx.interestedEvents_ & EPOLLIN) == 0) {
            result = ctx.isRegistered()
                ? epoller_.mod(fd, newEvents)
                : epoller_.add(fd, newEvents);
        }

        if (!result) {
            ctx.read_.handle_ = {}; // 修改失败，清空句柄
            return Result<void, std::error_code>::failure(
                result.error());
        }

        ctx.interestedEvents_ = newEvents; // epoll先添加成功 才能标记注册
        return Result<void, std::error_code>::success();
    }

    // 读回调重载
    Result<void, std::error_code> addReadEvent(int fd,
                                               std::function<void()> callback) {
        auto& ctx = fdContexts_[fd];
        ctx.fd_ = fd;

        if (ctx.read_.handle_ || ctx.read_.callback_) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }
        ctx.read_.callback_ = std::move(callback);

        uint32_t newEvents = ctx.interestedEvents_ | EPOLLIN | EPOLLET;
        Result<void, std::error_code> result = Result<void, std::error_code>::success();
        if ((ctx.interestedEvents_ & EPOLLIN) == 0) {
            result = ctx.isRegistered()
                ? epoller_.mod(fd, newEvents)
                : epoller_.add(fd, newEvents);
        }

        if (!result) {
            ctx.read_.callback_ = nullptr;
            return Result<void, std::error_code>::failure(
                result.error());
        }

        ctx.interestedEvents_ = newEvents;
        return Result<void, std::error_code>::success();
    }

    // 某个 fd 已经注册在 epoll + fdContexts 里，但还没来得及放进 connections
    Result<void, std::error_code> remove(int fd) {
        auto fdIt = fdContexts_.find(fd);
        if (fdIt == fdContexts_.end())
            return Result<void, std::error_code>::success();

        auto delResult = epoller_.del(fd);
        if (!delResult) {
            return Result<void, std::error_code>::failure(
                delResult.error());
        }
        fdContexts_.erase(fdIt);

        auto conIt = connections_.find(fd);
        if (conIt != connections_.end())
            connections_.erase(conIt);

        return Result<void, std::error_code>::success();
    }

    // 跨线程投递能力
    void pushConnfd(int connFd) {
        {
            std::lock_guard lock(pendingConnFdsMutex_);
            penddingConnFds_.push_back(connFd);
        }
        notifier_.notify();
    }

    // 是否管理某个连接
    bool hasConnection(int fd) const noexcept {
        return connections_.find(fd) != connections_.end();
    }

    std::shared_ptr<TcpConnection> findConnection(int fd) const {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return nullptr;

        return it->second;
    }

private:
    // 处理投递过来的连接
    void handlePendingConnFds() noexcept {
        std::deque<int> pendingFds;
        {
            std::lock_guard lock(pendingConnFdsMutex_);
            pendingFds.swap(penddingConnFds_);
        }
        for (int fd : pendingFds) {
            if (fd == -1)
                continue;

            auto conn = std::make_shared<TcpConnection>(fd);
            connections_[fd] = std::move(conn);
        }

    }


private:
    explicit EventLoop(Epoller &&epoller, EventNotifier &&notifier)
        : epoller_(std::move(epoller)),
          notifier_(std::move(notifier)) {
    }
private:
    Epoller epoller_;
    EventNotifier notifier_;
    TimerQueue timerQueue_;
    std::unordered_map<int, FdContext> fdContexts_ {};
    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_ {};

    std::mutex pendingConnFdsMutex_;
    std::deque<int> penddingConnFds_;

    bool running_{false};
};

}
