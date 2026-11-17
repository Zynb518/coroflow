#pragma once

#include <coroflow/TcpConnection.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <system_error>


namespace coroflow {
class EventLoop;

class TcpSession {
public:
    TcpSession(std::shared_ptr<TcpConnection> conn, EventLoop& loop) noexcept
        :conn_(std::move(conn)), loop_(&loop) {}

    int fd() const noexcept {
        return conn_->fd();
    }

    TcpConnection::State state() const noexcept {
        return conn_->state();
    }

    size_t readableBytes() const noexcept {
        return conn_->readableBytes();
    }

    bool hasPendingWrite() const noexcept {
        return conn_->hasPendingWrite();
    }

    std::string_view peekReadable() const noexcept {
        return conn_->peekReadable();
    }

    std::string retrieve(size_t len) {
        return conn_->retrieve(len);
    }

    std::string retrieveAll() {
        return conn_->retrieveAll();
    }

    void appendWrite(std::string_view data) {
        conn_->appendWrite(data);
    }

    Result<size_t, std::error_code> readFromFd() noexcept {
        return conn_->readFromFd();
    }

    Result<size_t, std::error_code> writeToFd() noexcept {
        return conn_->writeToFd();
    }

    void shutdownWrite() noexcept {
        conn_->shutdownWrite();
    }

    void close() noexcept {
        conn_->close();
    }

    TcpConnection& connection() noexcept {
        return *conn_;
    }

    const TcpConnection& connection() const noexcept {
        return *conn_;
    }

    EventLoop& loop() noexcept {
        return *loop_;
    }

    const EventLoop& loop() const noexcept {
        return *loop_;
    }


private:
    std::shared_ptr<TcpConnection> conn_;
    EventLoop* loop_;
};

}