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
        :handle(std::exchange(other.handle, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this == &other)
            return *this;

        if (handle)
            handle.destroy();

        handle = std::exchange(other.handle, {});
        return *this;
    }

    ~Task() {
        if (handle)
            handle.destroy();
    }

    // 协程句柄是否有效
    bool valid() const noexcept {
        return static_cast<bool>(handle);
    }
    // 协程是否已完成
    bool done() const noexcept {
        return (handle && handle.done());
    }
    // 恢复协程句柄
    void resume() noexcept {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }

    /* ---- promise_type ---- */
    /* 保存返回值，保存异常，保存父协程 continuation */
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation{};

        Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        // lazy 启动
        std::suspend_always initial_suspend() const noexcept {
            return {};
        }

        auto final_suspend() const noexcept {
            struct FinalAwaiter {
                bool await_ready() const noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(handle_type handle) const noexcept {
                    // 恢复父亲协程
                    auto continuation = handle.promise().continuation;
                    if (continuation)
                        return continuation;

                    return std::noop_coroutine();
                }

                void await_resume() const noexcept {}
            };

            return FinalAwaiter{};
        }

        // 转发参数
        template<typename U>
        void return_value(U&& result) {
            value.emplace(std::forward<U>(result));
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    /* ---- co_await 三件套 ---- */
    bool await_ready() const noexcept {
        return !handle || handle.done();
    }
    // 保存父协程
    handle_type await_suspend(std::coroutine_handle<> continuation) noexcept {
        handle.promise().continuation = continuation;
        return handle;
    }
    // 有异常重新抛出，没异常保存返回值
    T await_resume() {
        auto& promise = handle.promise();

        if (promise.exception)
            std::rethrow_exception(promise.exception);

        return std::move(*promise.value);
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
        if (this == &other)
            return *this;

        if (handle)
            handle.destroy();

        handle = std::exchange(other.handle, {});
        return *this;
    }

    ~Task() {
        if (handle)
            handle.destroy();
    }

    // 协程句柄是否有效
    bool valid() const noexcept {
        return static_cast<bool>(handle);
    }
    // 协程是否已完成
    bool done() const noexcept {
        return (handle && handle.done());
    }
    // 恢复协程句柄
    void resume() noexcept {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }

    /* ---- promise_type ---- */
    /* 不保存返回值，保存异常，保存父协程 continuation */
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation{};

        Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() const noexcept {
            return {};
        }

        auto final_suspend() const noexcept {
            struct FinalAwaiter {
                bool await_ready() const noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(handle_type handle) const noexcept {
                    if (auto continuation = handle.promise().continuation)
                        return continuation;

                    return std::noop_coroutine();
                }

                void await_resume() const noexcept {}
            };

            return FinalAwaiter{};
        }

        void return_void() const noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    /* ---- co_await 三件套 ---- */
    bool await_ready() const noexcept {
        return !handle || handle.done();
    }

    handle_type await_suspend(std::coroutine_handle<> continuation) noexcept {
        handle.promise().continuation = continuation;
        return handle;
    }

    void await_resume() {
        auto& promise = handle.promise();
        if (promise.exception)
            std::rethrow_exception(promise.exception);
    }

private:
    handle_type handle{};
};


}
