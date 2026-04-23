#pragma once

#include <coroutine>
#include <cstdint>
#include <functional>
namespace coroflow {

struct FdEventContext {
    std::coroutine_handle<> handle_{};
    std::function<void()> callback_;

    bool empty() const noexcept {
        return !handle_ && !callback_;
    }

    void reset() noexcept {
        handle_ = {};
        callback_ = nullptr;
    }
};

struct FdContext {
    int fd_{-1};
    uint32_t interestedEvents_{0};

    FdEventContext read_;
    FdEventContext write_;

    bool isRegistered() const noexcept {
        return interestedEvents_ != 0;
    }

    void reset() noexcept {
        fd_ = -1;
        interestedEvents_ = 0;
        read_.reset();
        write_.reset();
    }
};

}
