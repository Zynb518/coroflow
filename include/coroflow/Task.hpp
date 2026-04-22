#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace coroflow {

// 为了使用协程，必须自定义RAII 对象
template<typename T>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task() = default;

    explicit Task(handle_type handle) noexcept
        :handle(handle) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept
        :handle(std::exchange(other.handle), {}) {}

    Task& operator=(Task&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (handle) {
            handle.destroy();
        }

        handle = std::exchange(other.handle, {});
        return *this;
    }

    ~Task() {
        if (handle) {
            handle.destroy();
        }
    }

    bool valid() const noexcept {
        return static_cast<bool>(handle);
    }

private:
    handle_type handle;
};

template<>
class Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task() noexcept = default;

    explicit Task(handle_type handle) noexcept
        : handle(handle) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (handle) {
            handle.destroy();
        }

        handle = std::exchange(other.handle, {});
        return *this;
    }

    ~Task() {
        if (handle) {
            handle.destroy();
        }
    }

    bool valid() const noexcept {
        return static_cast<bool>(handle);
    }

protected:
    handle_type handle{};
};


}