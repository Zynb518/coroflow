# TcpServer -> HttpServer 分层与扩展方案

## 1. 设计目标

当前项目希望同时满足两种使用方式：

1. `TcpServer` 可以单独作为一个通用 TCP 服务框架使用。
2. `TcpServer` 可以继续向上封装，演化为 `HttpServer`、自定义二进制协议服务器、Redis 风格协议服务器等。

为了同时满足这两个目标，`TcpServer` 必须保持纯粹：

- 它负责监听、接收连接、连接分发、连接生命周期驱动。
- 它不直接理解 HTTP、WebSocket 或其他具体协议。
- 它不把业务逻辑写死在框架内部。

换句话说，`TcpServer` 应该是一个“传输层内核”，而 `HttpServer` 应该是基于它继续向上的“协议层封装”。

## 2. 核心原则

本方案遵循以下原则：

- `TcpServer` 只负责传输层，不负责协议层。
- `HttpServer` 通过组合 `TcpServer` 获得监听、accept、分发能力，而不是重新实现一套底层网络框架。
- 对上层的主要扩展点不是零散的 `read/write callback`，而是“连接会话入口”。
- 协议逻辑应尽量写成顺序式协程，而不是拆成碎片化的状态回调。

这意味着：

- `TcpServer` 的职责是“把一个新连接稳定地交给上层处理”。
- `HttpServer` 的职责是“定义这个连接接下来应该按 HTTP 协议如何运行”。

## 3. 分层结构

推荐把整体结构理解为三层。

### 3.1 运行时层

运行时层提供事件驱动与协程调度能力，包括：

- `Epoller`
- `EventLoop`
- `EventNotifier`
- `FdContext`
- `Task<T>`

这一层负责：

- 等待 IO 事件
- 保存协程句柄
- 在事件到来时恢复协程
- 提供跨线程唤醒能力

这一层不理解 TCP 连接的业务含义，也不理解 HTTP 请求。

### 3.2 传输层

传输层的核心对象包括：

- `TcpSocket`
- `TcpAcceptor`
- `TcpConnection`
- `TcpServer`
- `IoLoopSelector`
- `EventLoopThreadPool`

这一层负责：

- 创建监听 socket
- 绑定地址并开始监听
- accept 新连接
- 将连接分发到某个 worker `EventLoop`
- 创建连接对象
- 启动连接会话处理入口

这一层只处理“连接”和“字节流”。

### 3.3 协议层

协议层建立在传输层之上，典型对象可以是：

- `HttpServer`
- `RedisServer`
- `WsServer`

这一层负责：

- 协议解析
- 请求/消息路由
- 协议状态机
- 响应生成
- keep-alive / pipeline / framing 等协议细节

协议层不应重新实现监听、线程分发和连接调度。

## 4. TcpServer 的职责边界

### 4.1 TcpServer 应该负责什么

`TcpServer` 应负责以下事情：

1. 创建和持有主 `EventLoop`
2. 创建和持有 `TcpAcceptor`
3. 创建和持有 worker 线程池
4. 创建和持有 `IoLoopSelector`
5. 在主 loop 上持续接收新连接
6. 选择一个 worker loop 来接管该连接
7. 构造 `TcpConnection`
8. 在目标 worker loop 上启动连接会话处理逻辑

也就是说，`TcpServer` 是一个“连接驱动框架”。

### 4.2 TcpServer 不应该负责什么

`TcpServer` 不应负责：

- 解析 HTTP 请求
- 路由 URL
- 组装 HTTP 响应
- 解释应用层协议状态
- 承担业务层对象生命周期

如果把这些逻辑都塞进 `TcpServer`，它就不再是纯 TCP 服务框架，而会变成某种特定协议的专用 server。

## 5. 为什么不以读回调/写回调作为主扩展点

传统 Reactor 风格很容易把扩展接口设计成：

- `setReadCallback(...)`
- `setWriteCallback(...)`

这种方式不是完全错误，但它更适合回调式状态机，不太适合作为当前项目的主扩展模型。

原因如下：

- 一个连接的真实业务往往不是“收到一次可读事件就结束”，而是一整段会话。
- 协议处理通常需要多次读取、多次写入，以及中间状态保存。
- 如果只暴露 `read/write callback`，上层就必须自己维护状态机，代码会碎片化。
- 你当前项目已经明确在往协程风格走，顺序式协程会比回调更贴合协议会话表达。

因此，这里建议：

- `read/write callback` 可以保留为底层机制或兼容扩展点。
- 但对上层暴露的主扩展点应是“连接会话协程入口”。

## 6. 推荐的主扩展点

推荐 `TcpServer` 只暴露一个统一的连接处理入口，例如语义上类似：

```cpp
using ConnectionHandler =
    std::function<Task<void>(std::shared_ptr<TcpConnection>, EventLoop&)>;
```

这个扩展点的意义是：

- 上层拿到的是一个已经建立好的连接对象，而不是裸 `fd`
- 上层拿到当前 worker `EventLoop` 上下文
- 上层可以在一个协程中自由组织读取、写入、协议解析和错误处理
- `TcpServer` 不需要知道这个 handler 内部是在处理 HTTP 还是别的协议

这比暴露两个零散回调更完整，也更容易向 `HttpServer` 演进。

## 7. TcpServer 的运行模型

推荐的运行流程如下：

1. `TcpServer::create(...)`
   - 创建主 `EventLoop`
   - 创建 `TcpAcceptor`
   - 创建 worker `EventLoopThreadPool`
   - 初始化 `IoLoopSelector`
   - 保存上层注入的 `ConnectionHandler`

2. `TcpServer::run()`
   - 启动主接收协程
   - 启动 worker 线程池
   - 启动主 loop

3. 主接收协程
   - 在主 loop 上持续 `co_await acceptor.accept(mainLoop)`
   - 每拿到一个 `connFd`，通过 selector 选择目标 worker loop
   - 在目标 worker loop 上构造 `TcpConnection`
   - 启动用户提供的 `ConnectionHandler`

4. 会话协程
   - 从这里开始，连接进入协议层或业务层逻辑

这个模型的关键是：

- `TcpServer` 只负责把连接“交出去”
- 上层 handler 决定连接“怎么活”

## 8. HttpServer 如何基于 TcpServer 封装

`HttpServer` 不应继承 `TcpServer` 去改写其内部行为，而应通过组合来使用它。

推荐结构如下：

```text
HttpServer
└── TcpServer
    └── ConnectionHandler (由 HttpServer 提供)
```

### 8.1 HttpServer 的职责

`HttpServer` 负责：

- 定义 HTTP 专用的连接会话逻辑
- 解析请求
- 路由匹配
- 调用 controller / handler
- 生成响应
- 管理 keep-alive / pipeline / upgrade 等 HTTP 语义

### 8.2 HttpServer 不负责什么

`HttpServer` 不应重新做：

- 主监听
- accept
- worker 分发
- 底层线程池管理
- socket 生命周期底层管理

这些都应继续交给 `TcpServer`。

### 8.3 HttpServer 的接入方式

`HttpServer` 在初始化时，内部创建一个 `TcpServer`，然后为它提供一个 HTTP 版本的 `ConnectionHandler`。

这个 handler 的逻辑可以是：

1. 循环从 `TcpConnection` 读取字节流
2. 解析成 HTTP 请求
3. 调用路由系统
4. 生成响应
5. 写回连接
6. 根据连接头决定是否继续 keep-alive

这样 `HttpServer` 的能力完全建立在 `TcpServer` 之上，但 `TcpServer` 本身不会被污染成 HTTP 专用组件。

## 9. 为什么选择组合而不是继承

这里推荐组合而不是继承，原因如下：

- `TcpServer` 和 `HttpServer` 的关系不是“是一个更特殊的 TcpServer”，而是“使用一个 TcpServer 实现自己的协议层能力”。
- 继承会让 `TcpServer` 暴露过多可被覆写的内部细节，破坏边界。
- 组合更容易保持传输层稳定，也更方便以后出现第三种、第四种协议层封装。

使用组合后：

- `TcpServer` 保持稳定
- `HttpServer` 只负责增加协议逻辑
- 以后新增别的协议 server 时不需要动传输层

## 10. 当前阶段建议

结合当前项目进度，建议的实现顺序是：

1. 先把 `TcpServer` 做成纯传输层组件
2. 为 `TcpServer` 定义统一的 `ConnectionHandler`
3. 先验证：
   - 能 accept
   - 能分发到 worker loop
   - 能在 worker loop 上启动一个连接协程
4. 暂不引入 HTTP 解析
5. 等 `TcpServer` 跑通后，再用组合方式设计 `HttpServer`

这样可以避免在 `TcpServer` 尚未稳定时，同时把 HTTP 层逻辑也混进来，导致问题难以定位。

## 11. 当前设计结论

本方案的最终结论是：

- `TcpServer` 是纯传输层连接驱动框架
- `TcpServer` 的主扩展点应是“连接会话协程入口”
- `HttpServer` 通过组合 `TcpServer` 来获得网络能力
- `HttpServer` 负责 HTTP 协议层逻辑，而不是改造底层传输层
- 读写回调不是主扩展模型，连接会话协程才是主扩展模型

如果后续需要新增其他协议 server，也沿用同一模式：

- 复用 `TcpServer`
- 注入不同的 `ConnectionHandler`
- 在协议层实现不同的会话逻辑

