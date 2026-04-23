#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace coroflow {

enum class LogLevel {
    Info,
    Warn,
    Error,
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        std::ostream& out = (level == LogLevel::Error) ? std::cerr : std::cout;

        out << '[' << currentTime() << "] "
            << '[' << levelToString(level) << "] "
            << "[TID:" << std::this_thread::get_id() << "] "
            << message << '\n';
    }

private:
    Logger() {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        std::cerr.tie(nullptr);
    }

    static const char* levelToString(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warn:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
        }

        return "UNKNOWN";
    }

    static std::string currentTime() {
        auto now = std::chrono::system_clock::now();
        auto timePoint = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tmNow{};
#ifdef _WIN32
        localtime_s(&tmNow, &timePoint);
#else
        localtime_r(&timePoint, &tmNow);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

private:
    std::mutex mutex;
};

class LogStream {
public:
    explicit LogStream(LogLevel level) noexcept
        : level(level) {
    }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    LogStream(LogStream&& other) noexcept
        : level(other.level),
          oss(std::move(other.oss)) {
    }

    ~LogStream() {
        Logger::instance().log(level, oss.str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        oss << value;
        return *this;
    }

    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        oss << manip;
        return *this;
    }

private:
    LogLevel level;
    std::ostringstream oss;
};

} // namespace coroflow

#define LOG_INFO  ::coroflow::LogStream(::coroflow::LogLevel::Info)
#define LOG_WARN  ::coroflow::LogStream(::coroflow::LogLevel::Warn)
#define LOG_ERROR ::coroflow::LogStream(::coroflow::LogLevel::Error)
