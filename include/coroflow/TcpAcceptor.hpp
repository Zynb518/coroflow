#pragma once

#include <coroflow/TcpSocket.hpp>
#include <coroflow/SocketAddress.hpp>

namespace coroflow {

class TcpAcceptor {
public:

private:
    TcpSocket socket;
    SocketAddress address;
};

}