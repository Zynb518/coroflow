#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace coroflow {

template<typename T, typename E>
class Result {
    static_assert(!std::is_same_v<T, E>,
        "Result<T, E> requires T and E to be different types");

public:

    // 为了转发，必须引入新的模板参数，类模板在初始化的时候已经是确认类型 T&& 不是万能引用
    template<typename U>
    static Result success(U&& value) {
        return Result(SuccessTag{}, std::forward<U>(value));
    }

    // 同上
    template<typename F>
    static Result failure(F&& error) {
        return Result(ErrorTag{}, std::forward<F>(error));
    }

    bool hasValue () const {
        return std::holds_alternative<T>(storage);
    }

    explicit operator bool() const {
        return hasValue();
    }

    // 同一件事发生在不同对象状态上该返回什么值，当Result对象为左值，常量左值，右值时返回什么值
    T& value() & {
        if (!hasValue()) {
            throw std::logic_error("Result does not contain a value");
        }
        return std::get<T>(storage);
    }

    const T& value() const& {
        if (!hasValue()) {
            throw std::logic_error("Result does not contain a value");
        }
        return std::get<T>(storage);
    }

    T&& value() && {
        if (!hasValue()) {
            throw std::logic_error("Result does not contain a value");
        }
        return std::get<T>(std::move(storage));
    }

    // 同 value
    E& error() & {
        if (hasValue()) {
            throw std::logic_error("Result does not contain an error");
        }
        return std::get<E>(storage);
    }

    const E& error() const& {
        if (hasValue()) {
            throw std::logic_error("Result does not contain an error");
        }
        return std::get<E>(storage);
    }

    E&& error() && {
        if (hasValue()) {
            throw std::logic_error("Result does not contain an error");
        }
        return std::get<E>(std::move(storage));
    }

private:
    struct SuccessTag{};
    struct ErrorTag{};

    template<typename U>
    explicit Result(SuccessTag, U&& value)
        :storage(std::forward<U>(value)) {}

    template<typename F>
    explicit Result(ErrorTag, F&& error)
        :storage(std::forward<F>(error)) {}

private:
    std::variant<T, E> storage;
};

// 对主模板的特化
template<typename E>
class Result<void, E> {
public:
    static Result success() {
        return Result(true, E{});
    }

    template<typename F>
    static Result failure(F&& error) {
        return Result(false, std::forward<F>(error));
    }

    bool hasValue() const {
        return ok;
    }

    explicit operator bool() const {
        return hasValue();
    }

    // value 只有一种 因为没有 void& 这些
    void value() const {
        if (!ok) {
            throw std::logic_error("Result does not contain a value");
        }
    }

    // error 依旧两种
    E& error() & {
        if (ok) {
            throw std::logic_error("Result does not contain an error");
        }
        return errorValue;
    }

    const E& error() const& {
        if (ok) {
            throw std::logic_error("Result does not contain an error");
        }
        return errorValue;
    }

    E&& error() && {
        if (ok) {
            throw std::logic_error("Result does not contain an error");
        }
        return std::move(errorValue);
    }

private:
    template<typename F>
    Result(bool ok, F&& error)
        : ok(ok), errorValue(std::forward<F>(error)) {}

private:
    bool ok;
    E errorValue;
};


}

