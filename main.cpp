#include <iostream>

#include <coroflow/Task.hpp>
#include <coroflow/TcpAcceptor.hpp>
#include <coroflow/EventLoop.hpp>

#include <unistd.h>

coroflow::Task<void> acceptLoop(coroflow::TcpAcceptor &acceptor,
                                coroflow::EventLoop &loop) {
    for (;;) {
        auto acceptResult = co_await acceptor.accept(loop);
        if (!acceptResult)
            continue;

        int connFd = acceptResult.value();
        ::close(connFd);
    }
}

int main() {
    auto loopResult = coroflow::EventLoop::create();

    if (!loopResult) {
        std::cerr << "create event loop failed: "
                  << loopResult.error().message() << '\n';
        return 1;
    }

    auto acceptorResult = coroflow::TcpAcceptor::create();
    if (!acceptorResult) {
        std::cerr << "create acceptor failed: "
                  << acceptorResult.error().message() << '\n';
        return 1;
    }

    auto addressResult = coroflow::SocketAddress::ipV4("0.0.0.0", 8080);
    if (!addressResult) {
        std::cerr << "create address failed: "
                  << addressResult.error().message() << '\n';
        return 1;
    }

    auto listenResult = acceptorResult.value().listen(addressResult.value());
    if (!listenResult) {
        std::cerr << "listen failed: "
                  << listenResult.error().message() << '\n';
        return 1;
    }

    auto task = acceptLoop(acceptorResult.value(), loopResult.value());
    task.resume();

    loopResult.value().run();
    return 0;
}
