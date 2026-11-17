#include <coroutine>
#include <iostream>
#include <system_error>

#include <coroflow/TcpServer.hpp>
#include <coroflow/FdAwaiter.hpp>

namespace cf = coroflow;

cf::Task<void> echoSession(cf::TcpSession session) {
    for (;;) {
        auto waitResult = co_await waitReadable(session.loop(), session.fd());
        if (!waitResult) {
            std::cerr << "wait readable failed: "
                    << waitResult.error().message() << '\n';
            co_return;
        }

        auto readResult = session.readFromFd();
        if (!readResult) {
            if (readResult.error() ==
                std::make_error_code(std::errc::resource_unavailable_try_again)) {
                continue;
            }

            std::cerr << "read failed: "
                    << readResult.error().message() << '\n';
            session.close();
            co_return;
        }

        if (readResult.value() == 0) {
            std::cout << "peer closed\n";
            session.close();
            co_return;
        }

        auto data = session.retrieveAll();
        std::cout << "recv: " << data << '\n';

        session.appendWrite(data);

        auto writeResult = session.writeToFd();
        if (!writeResult) {
            std::cerr << "write failed: "
                    << writeResult.error().message() << '\n';
            session.close();
            co_return;
        }
    }
}

int main() {
    auto addrResult = cf::SocketAddress::ipV4("0.0.0.0", 10086);
    if (!addrResult) {
        std::cerr << "create address failed: "
                << addrResult.error().message() << '\n';
        return 1;
    }

    auto serverResult = cf::TcpServer::create(std::move(addrResult).value(), 2);
    if (!serverResult) {
        std::cerr << "create server failed: "
                << serverResult.error().message() << '\n';
        return 1;
    }

    auto server = std::move(serverResult).value();
    server.setConnectionHandler(echoSession);

    std::cout << "server listening on 0.0.0.0:10086\n";

    auto runResult = server.run();
    if (!runResult) {
        std::cerr << "server run failed: "
                << runResult.error().message() << '\n';
        return 1;
    }

    return 0;
}
