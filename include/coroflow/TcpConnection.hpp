#pragma once

#include "coroflow/SocketAddress.hpp"
#include "coroflow/Buffer.hpp"

namespace coroflow {
class TcpConnection {
public:

private:
    int fd;
    SocketAddress peerAddress;
    Buffer readBuffer;
    Buffer writeBuffer;
};
}