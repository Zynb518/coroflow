# coroflow 设计文档 V1

```text
用户层 / 协议层
└── Task<T> + co_await

传输层
├── TcpAcceptor   -> co_await accept
└── TcpConnection -> co_await read / write

运行时层
├── EventLoop
├── Epoller
├── FdContext
├── TimerQueue
└── EventNotifier
```


