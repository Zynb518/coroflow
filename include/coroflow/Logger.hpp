// simple_logger.h
#ifndef SIMPLE_LOGGER_H
#define SIMPLE_LOGGER_H

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <thread>

enum class LogLevel {
    INFO,
    WARN,
    ERROR
};

class Logger;

// 日志流辅助类：收集 << 输入，析构时提交
class LogStream {
public:
    LogStream(LogLevel level) : level_(level) {}

    // 移动构造（用于宏返回临时对象）
    LogStream(LogStream&& other) noexcept
        : level_(other.level_), oss_(std::move(other.oss_)) {}

    // 禁止拷贝
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream() {
        Logger::instance().log(level_, oss_.str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        oss_ << value;
        return *this;
    }

    // 支持 std::endl、std::flush 等操纵符
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        oss_ << manip;
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream oss_;
};

// 线程安全的日志记录器（单例）
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostream& out = (level == LogLevel::ERROR) ? std::cerr : std::cout;

        // 输出格式：[时间戳] [级别] [线程ID] 消息
        out << '[' << currentTime() << "] "
            << '[' << levelToString(level) << "] "
            << "[TID:" << std::this_thread::get_id() << "] "
            << message << '\n';   // 仅换行，不强制刷新
    }

private:
    Logger() {
        // 解除与 C stdio 的同步，提高 I/O 性能
        std::ios::sync_with_stdio(false);
        // 可选：解绑 cin 与 cout（如果不需要交互式输入）
        std::cin.tie(nullptr);
        std::cerr.tie(nullptr);
    }

    std::mutex mutex_;

    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default:              return "UNKN ";
        }
    }

    static std::string currentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;

        std::tm tm_now;
#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
};

// 便捷宏：创建临时 LogStream 对象
#define LOG_INFO   LogStream(LogLevel::INFO)
#define LOG_WARN   LogStream(LogLevel::WARN)
#define LOG_ERROR  LogStream(LogLevel::ERROR)

#endif // SIMPLE_LOGGER_H