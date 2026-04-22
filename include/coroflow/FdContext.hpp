#pragma once

#include <coroutine>
#include <cstdint>
#include <functional>
namespace coroflow {

struct FdEventContext {
    std::coroutine_handle<> handle{};
    std::function<void()> callback;

    bool empty() const noexcept {
        return !handle && !callback;
    }

    void reset() noexcept {
        handle = {};
        callback = nullptr;
    }
};

struct FdContext {
    int fd{-1};
    uint32_t interestedEvents{0};

    FdEventContext read;
    FdEventContext write;

    bool isRegistered() const noexcept {
        return interestedEvents != 0;
    }

    void reset() noexcept {
        fd = -1;
        interestedEvents = 0;
        read.reset();
        write.reset();
    }
};

}
