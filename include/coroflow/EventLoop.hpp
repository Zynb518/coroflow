#pragma once

#include <coroflow/Epoller.hpp>
#include <coroflow/TcpConnection.hpp>
#include <coroflow/EventNotifier.hpp>
#include <coroflow/TimerQueue.hpp>
#include <coroflow/Result.hpp>

#include <vector>
#include <array>
#include <coroutine>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "FdContext.hpp"

namespace coroflow {

class EventLoop {
public:
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop &&) = default;
    EventLoop& operator=(EventLoop &&) = default;

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

        auto addResult = loop.epoller.add(loop.eventNotifier.fd(), EPOLLIN | EPOLLET);
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
        running = true;

        while (running) {
            auto waitResult = epoller.wait(epEvents.data(),
                                           epEvents.size(), -1); // 阻塞 wait
            if (!waitResult)
                continue;
            int readyCount = waitResult.value();
            for (int i = 0; i < readyCount; ++i) {
                int fd = epEvents[i].data.fd;
                // 被唤醒，单独处理
                if (fd == eventNotifier.fd()) {
                    eventNotifier.handleRead();
                    continue;
                }

                uint32_t revents = epEvents[i].events;

                auto it = fdContexts.find(fd);
                if (it == fdContexts.end())
                    continue;

                auto& ctx = it->second;
                if (revents & EPOLLIN) {
                    if (ctx.read.handle) {
                        auto handle = std::exchange(ctx.read.handle, std::coroutine_handle<>{});
                        handle.resume();
                    } else if (ctx.read.callback) {
                        ctx.read.callback();
                    }
                }
            } // for

        } // while
    }

    void stop() noexcept {
        running = false;
        eventNotifier.notify();
    }

    // “epoll 永久订阅、协程一次性消费”
    Result<void, std::error_code> addReadEvent(int fd,
                                               std::coroutine_handle<> handle) {
        auto& ctx = fdContexts[fd];
        ctx.fd = fd;

        // 已经有句柄或者回调 不能重复注册
        if (ctx.read.handle || ctx.read.callback) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }
        ctx.read.handle = handle;

        uint32_t newEvents = ctx.interestedEvents | EPOLLIN | EPOLLET; // 新的感兴趣的事件
        auto result = Result<void, std::error_code>::success();
        // 有interestedEvents = 已经注册epoll
        if ((ctx.interestedEvents & EPOLLIN) == 0) {
            result = ctx.isRegistered()
                ? epoller.mod(fd, newEvents)
                : epoller.add(fd, newEvents);
        }

        if (!result) {
            ctx.read.handle = {}; // 修改失败，清空句柄
            return Result<void, std::error_code>::failure(
                result.error());
        }

        ctx.interestedEvents = newEvents; // epoll先添加成功 才能标记注册
        return Result<void, std::error_code>::success();
    }

    // 读回调重载
    Result<void, std::error_code> addReadEvent(int fd,
                                               std::function<void()> callback) {
        auto& ctx = fdContexts[fd];
        ctx.fd = fd;

        if (ctx.read.handle || ctx.read.callback) {
            return Result<void, std::error_code>::failure(
                std::make_error_code(std::errc::operation_in_progress));
        }
        ctx.read.callback = std::move(callback);

        uint32_t newEvents = ctx.interestedEvents | EPOLLIN | EPOLLET;
        Result<void, std::error_code> result = Result<void, std::error_code>::success();
        if ((ctx.interestedEvents & EPOLLIN) == 0) {
            result = ctx.isRegistered()
                ? epoller.mod(fd, newEvents)
                : epoller.add(fd, newEvents);
        }

        if (!result) {
            ctx.read.callback = nullptr;
            return Result<void, std::error_code>::failure(
                result.error());
        }

        ctx.interestedEvents = newEvents;
        return Result<void, std::error_code>::success();
    }


    Result<void, std::error_code> remove(int fd) {
        auto it = fdContexts.find(fd);
        if (it == fdContexts.end()) {
            return Result<void, std::error_code>::success();
        }

        auto delResult = epoller.del(fd);
        if (!delResult) {
            return Result<void, std::error_code>::failure(
                delResult.error());
        }

        fdContexts.erase(it);
        return Result<void, std::error_code>::success();
    }


private:
    explicit EventLoop(Epoller &&epoller, EventNotifier &&notifier)
        : epoller(std::move(epoller)),
          eventNotifier(std::move(notifier)) {
    }
private:
    Epoller epoller;
    EventNotifier eventNotifier;
    std::vector<TcpConnection> connections; // 管理的连接，暂时用vector
    TimerQueue timerQueue;

    std::unordered_map<int, FdContext> fdContexts;

    bool running{false};
};

}
