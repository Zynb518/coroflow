#pragma once

#include <netinet/in.h> // addr
#include <arpa/inet.h>  // inet_pton
#include <string>
#include <system_error>

#include "coroflow/Result.hpp"

namespace coroflow {

struct SocketAddress {
public:

    ~SocketAddress() = default;

    // 工厂函数
    static Result<SocketAddress, std::error_code> ipV4(const std::string &ip,
                                                       uint16_t port) {
        sockaddr_in address{};
        int ret = ::inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
        if (ret == -1) {
            return Result<SocketAddress, std::error_code>::failure(
                std::error_code(errno, std::generic_category()));
        }

        // 参数错误
        if (ret == 0) {
            return Result<SocketAddress, std::error_code>::failure(
                std::make_error_code(std::errc::invalid_argument));
        }

        address.sin_family = AF_INET;
        address.sin_port = ::htons(port);
        return Result<SocketAddress, std::error_code>::success(SocketAddress(address));
    }

    const sockaddr* sockaddrPtr() const noexcept {
        return reinterpret_cast<const sockaddr*>(&address_);
    }

    socklen_t sockaddrLen() const noexcept {
        return sizeof(address_);
    }

    std::string toString() const {
        char buf[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &address_.sin_addr, buf, INET_ADDRSTRLEN);
        return std::string(buf) + ":" + std::to_string(::ntohs(address_.sin_port));
    }

private:
    explicit SocketAddress(const sockaddr_in& address)
        : address_(address){}


private:
    sockaddr_in address_{};
};

}