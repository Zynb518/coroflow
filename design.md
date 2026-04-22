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



# 附录 STL 相关补充

## **std::is_same_v**

```cpp
template<typename T, T v>
struct integral_constant {
	static constexpr T value = v;
	using value_type = T;
	using type = integral_constant;
	
	constexpr operator value_type() const noexcept {
	    return value;
	}
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template<typename T, typename U>
struct is_same : false_type {};

template<typename T>
struct is_same<T, T> : true_type {};

template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;
```

这段代码的核心非常直接：is_same 的主模板先假定“两个类型不一样”，所以继承 false_type，而template<typename T> struct is_same<T, T> : true_type {}; 是一个偏特化，意思是“只要两个位置上的类型其实是同一个 T，那结果就改成真”；于是当编译器看到 is_same<int, int> 时，偏特化能匹配，结果就是true_type，它里面的 value 是 true，而 is_same<int, double> 时，is_same<T, T> 根本匹配不上，因为一个 T 不可能同时推导成 int 和 double，所以只能落回主模板，结果就是 false_type；最后 is_same_v<T, U> 只是个语法糖，等价于 is_same<T, U>::value，让你不用每次都写很长一串模板名字。



## **std::holds_alternative**

把它理解成“先在编译期算出 T 在 variant<Types...> 里的下标，再在运行时看当前激活下标是不是它”，一个接近实现原理的最小版本大概是这样：

```cpp
template<typename T, typename... Types>
constexpr bool holds_alternative(const std::variant<Types...>& v) noexcept {
	static_assert(((std::is_same_v<T, Types> ? 1 : 0) + ...) == 1,
	"T must appear exactly once in variant");
	return v.index() == find_index_v<T, Types...>;
}
```

== 1；而前面的 static_assert 是为了防止歧义，因为如果 T 在候选类型里没出现，或者出现了两次，holds_alternative<T> 就没法定义成一个明确的“检查当前是不是这个唯一类型”。



## std::variant

`std::variant` 你可以先把它理解成“类型安全版的联合体”，它在任意时刻只会保存候选类型里的一个值，但它会自己记住当前到底存的是哪一种类型，所以比 `union` 安全得多；对你现在的 `Result<T, E>` 来说，用 `std::variant<T, E>` 的意思就是“这个对象要么存成功值 `T`，要么存错误值 `E`”。

### 1. 最基本的定义和赋值

```cpp
#include <variant>
#include <string>

std::variant<int, std::string> v;

v = 42;
v = std::string("hello");
```

这里 `v` 的候选类型是 `int` 和 `std::string`，但同一时刻只能真的持有一个。

如果你直接默认构造：

```cpp
std::variant<int, std::string> v;
```

那么它默认会构造第一个类型，也就是 `int`，所以一开始 `v` 里存的是 `0`。

如果第一个类型不能默认构造，可以用 `std::monostate` 垫一个空状态：

```cpp
std::variant<std::monostate, int, std::string> v;
```

这在某些需要“初始为空”的场景里很常见。

### 2. 怎么判断当前存的是什么

最常用的是 `std::holds_alternative<T>(v)`：

```cpp
if (std::holds_alternative<int>(v)) {
    // 当前存的是 int
}
```

它适合你这种“先判断，再取值”的写法。

还有一个底层一点的接口是 `index()`：

```cpp
std::size_t i = v.index();
```

它返回当前激活的是第几个候选类型：

- `int` 是第 `0` 个
- `std::string` 是第 `1` 个

但平时业务代码里，直接看 `index()` 可读性不如 `holds_alternative<T>()`。

### 3. 怎么把值取出来

最直接的是 `std::get<T>(v)`：

```cpp
int x = std::get<int>(v);
```

或者按下标取：

```cpp
int x = std::get<0>(v);
```

但这里要注意，`get` 要求你取的类型必须正好是当前激活的那个类型，否则会抛 `std::bad_variant_access`：

```cpp
std::variant<int, std::string> v = 42;

std::string s = std::get<std::string>(v); // 抛异常
```

所以更稳的写法通常是先判断：

```cpp
if (std::holds_alternative<int>(v)) {
    int x = std::get<int>(v);
}
```

### 4. 更安全的取值方式：`std::get_if`

如果你不想碰异常，可以用 `std::get_if`：

```cpp
if (auto* p = std::get_if<int>(&v)) {
    // p 是 int*
    // *p 就是里面的值
}
```

如果当前不是 `int`，它会返回 `nullptr`，不会抛异常。

这在你以后写框架代码时通常比 `get<T>` 更稳，因为它更显式。

### 5. 怎么原地构造某个类型

除了赋值，还可以用 `emplace` 直接在 `variant` 内部构造目标对象：

```cpp
std::variant<int, std::string> v;

v.emplace<int>(123);
v.emplace<std::string>("hello");
```

这在对象比较重、你不想先构造再移动时比较有用。

也可以按下标：

```cpp
v.emplace<1>("world");
```

这里 `1` 对应的是 `std::string`。

### 6. 最重要的高级用法：`std::visit`

`std::visit` 是 `variant` 最有价值的地方，因为它允许你“根据当前实际类型自动分派处理逻辑”。

```cpp
std::variant<int, std::string> v = std::string("hello");

std::visit([](const auto& value) {
    std::cout << value << '\n';
}, v);
```

这里的 lambda 会根据当前存的是 `int` 还是 `std::string` 自动接到对应类型的参数。

如果你想区分不同类型的处理，可以这样写：

```cpp
std::visit([](const auto& value) {
    using ValueType = std::decay_t<decltype(value)>;

    if constexpr (std::is_same_v<ValueType, int>) {
        std::cout << "int: " << value << '\n';
    } else if constexpr (std::is_same_v<ValueType, std::string>) {
        std::cout << "string: " << value << '\n';
    }
}, v);
```

这类写法你后面会经常见到，因为它把“运行时分支”转成了“编译期类型分派”。

### 7. 结合 `Result<T, E>` 怎么理解

如果你写：

```cpp
std::variant<T, E> storage_;
```

那它就表示：

- 成功时，`storage_` 里装一个 `T`
- 失败时，`storage_` 里装一个 `E`

然后：

```cpp
bool hasValue() const {
    return std::holds_alternative<T>(storage_);
}
```

就是“当前是不是成功值”。

```cpp
T& value() {
    return std::get<T>(storage_);
}
```

就是“把成功值拿出来”。

```cpp
E& error() {
    return std::get<E>(storage_);
}
```

就是“把错误值拿出来”。

这也是为什么我前面让你加：

```cpp
static_assert(!std::is_same_v<T, E>);
```

因为如果 `T` 和 `E` 是同一个类型，比如都是 `int`，那 `std::variant<int, int>` 虽然语法上可以存在，但你用 `holds_alternative<int>` 或 `get<int>` 就会变得歧义，完全不适合 `Result`。

### 8. 常见坑

```cpp
std::variant<int, int> v;
```

这种写法要避免。重复类型会让按类型访问变得不明确。

```cpp
std::get<T>(v)
```

不要在没判断的情况下乱用，否则类型不匹配就抛异常。

```cpp
std::variant<Heavy, Error> v;
```

如果类型对象很重，赋值时可能发生移动构造或拷贝构造，要注意代价，但对你的 `TcpSocket` 这种 move-only RAII 类型是可以用的。

`std::variant` 不能拿来装引用类型，这点也要知道；如果真要表达引用关系，通常放指针或 `std::reference_wrapper`。

### 9. 接口总结

你现在先只吃透这几个就够了：

- `std::variant<T, E>`
- `std::holds_alternative<T>(v)`
- `std::get<T>(v)`
- `std::get_if<T>(&v)`
- `v.emplace<T>(...)`
- `std::visit(...)`

## std::exchange

`std::exchange` 的作用很直接：先把对象当前的旧值取出来返回，再把对象改成一个新值。它特别适合资源转移场景，比如移动构造、移动赋值，或者“接管资源后把源对象置空”这种操作。

一个接近原理的最小版本可以写成这样：

```cpp
template<class T, class U = T>
constexpr T exchange(T& obj, U&& new_value) {
    T old = std::move(obj);
    obj = std::forward<U>(new_value);
    return old;
}
```

它的执行顺序是固定的：先把 `obj` 当前的值保存到 `old`，再把 `new_value` 赋给 `obj`，最后把旧值返回出去。

## std::make_error_code 与 std::errc::invalid_argument

`std::make_error_code(std::errc::invalid_argument)` 的作用是构造一个标准库风格的 `std::error_code` 对象，用来表达“参数非法”这类错误。它比直接手写一个整数错误码更规范，因为返回值里同时带了错误值和错误类别，后面可以直接参与统一的 `Result<T, std::error_code>` 错误链。

对应的典型写法是：

```cpp
std::error_code ec = std::make_error_code(std::errc::invalid_argument);
```

这里的 `std::errc` 是一个标准枚举，里面放的是一组可移植的通用错误语义，比如：

```cpp
std::errc::invalid_argument
std::errc::address_in_use
std::errc::not_supported
```

而 `std::make_error_code(...)` 会把这些枚举值转换成真正的 `std::error_code` 对象，所以你后面就可以正常调用：

```cpp
ec.value();
ec.message();
ec.category().name();
```

在你当前这个项目里，它特别适合处理“不是系统调用失败，而是你自己发现参数不合法”的场景。比如 `inet_pton` 返回 `0` 时，含义不是“系统调用出错”，而是“传入的 IP 字符串格式不合法”，这种情况就不应该去读 `errno`，而应该主动返回：

```cpp
return Result<SocketAddress, std::error_code>::failure(
    std::make_error_code(std::errc::invalid_argument)
);
```

这和下面这种用法要区分开：

```cpp
std::error_code(errno, std::generic_category())
```

后者表示“底层系统接口真的失败了，而且错误信息来自当前线程里的 `errno`”；前者表示“程序主动判定输入参数非法，即使没有系统错误，也可以稳定返回一个标准错误码”。

## std::error_code(errno, std::generic_category())

`std::error_code(errno, std::generic_category())` 的作用是把当前线程里的 `errno` 封装成一个标准库对象，方便你沿着 `Result<T, std::error_code>` 这条错误链继续往上传递。它适合用在真正失败的系统调用之后，比如 `socket`、`bind`、`listen`、`accept`、`close`、`inet_pton` 返回 `-1` 这种情况。

典型写法如下：

```cpp
if (::socket(AF_INET, SOCK_STREAM, 0) == -1) {
    return Result<TcpSocket, std::error_code>::failure(
        std::error_code(errno, std::generic_category())
    );
}
```

这里有两个部分：

```cpp
errno
std::generic_category()
```

`errno` 是系统调用失败后留下的错误号，比如 `EADDRINUSE`、`EBADF`、`EACCES`；`std::generic_category()` 则告诉 `std::error_code`，“这个错误值属于通用的 POSIX / C 风格错误类别”。

构造出来以后，你就可以统一地读取它：

```cpp
std::error_code ec(errno, std::generic_category());

ec.value();
ec.message();
ec.category().name();
```

其中：

- `value()` 返回底层整数错误码
- `message()` 返回可读错误描述
- `category().name()` 返回错误类别名称

它和 `std::make_error_code(std::errc::invalid_argument)` 的区别在于：`std::error_code(errno, std::generic_category())` 是“把系统已经产生的错误封装起来”，而 `std::make_error_code(std::errc::invalid_argument)` 是“程序主动构造一个标准错误语义”。

因此你可以简单记成：

- 系统调用失败并且要读取 `errno`：用 `std::error_code(errno, std::generic_category())`
- 参数非法、状态非法、你自己主动判定失败：用 `std::make_error_code(std::errc::...)`





# 附录 系统API

```cpp
/* Create a new socket of type TYPE in domain DOMAIN, using
   protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
   Returns a file descriptor for the new socket, or -1 for errors.  */
extern int socket (int __domain, int __type, int __protocol) __THROW;
```



```cpp
/* Give the socket FD the local address ADDR (which is LEN bytes long).  */
extern int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
     __THROW;

```



```cpp
/* Prepare to accept connections on socket FD.
   N connection requests will be queued before further requests are refused.
   Returns 0 on success, -1 for errors.  */
extern int listen (int __fd, int __n) __THROW;

```



```cpp
/* Close the file descriptor FD.
   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int close (int __fd);
```







```cpp
/* Convert from presentation format of an Internet number in buffer
   starting at CP to the binary network format and store result for
   interface type AF in buffer starting at BUF.  */
extern int inet_pton (int __af, const char *__restrict __cp,
		      void *__restrict __buf) __THROW;

/* Convert a Internet address in binary network format for interface
   type AF in buffer starting at CP to presentation form and place
   result in buffer of length LEN astarting at BUF.  */
extern const char *inet_ntop (int __af, const void *__restrict __cp,
			      char *__restrict __buf, socklen_t __len)
     __THROW;
```



```cpp
/* Functions to convert between host and network byte order.

   Please note that these functions normally take `unsigned long int' or
   `unsigned short int' values as arguments and also return them.  But
   this was a short-sighted decision since on different systems the types
   may have different representations but the values are always the same.  */

extern uint32_t ntohl (uint32_t __netlong) __THROW __attribute__ ((__const__));
extern uint16_t ntohs (uint16_t __netshort)
     __THROW __attribute__ ((__const__));
extern uint32_t htonl (uint32_t __hostlong)
     __THROW __attribute__ ((__const__));
extern uint16_t htons (uint16_t __hostshort)
     __THROW __attribute__ ((__const__));
```



```cpp
/* Creates an epoll instance.  Returns an fd for the new instance.
   The "size" parameter is a hint specifying the number of file
   descriptors to be associated with the new instance.  The fd
   returned by epoll_create() should be closed with close().  */
extern int epoll_create (int __size) __THROW;

/* Same as epoll_create but with an FLAGS parameter.  The unused SIZE
   parameter has been dropped.  */
extern int epoll_create1 (int __flags) __THROW;
```



```cpp
/* Manipulate an epoll instance "epfd". Returns 0 in case of success,
   -1 in case of error ( the "errno" variable will contain the
   specific error code ) The "op" parameter is one of the EPOLL_CTL_*
   constants defined above. The "fd" parameter is the target of the
   operation. The "event" parameter describes which events the caller
   is interested in and any associated user data.  */
extern int epoll_ctl (int __epfd, int __op, int __fd,
		      struct epoll_event *__event) __THROW;
```



```cpp
/* Wait for events on an epoll instance "epfd". Returns the number of
   triggered events returned in "events" buffer. Or -1 in case of
   error with the "errno" variable set to the specific error code. The
   "events" parameter is a buffer that will contain triggered
   events. The "maxevents" is the maximum number of events to be
   returned ( usually size of "events" ). The "timeout" parameter
   specifies the maximum wait time in milliseconds (-1 == infinite).

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int epoll_wait (int __epfd, struct epoll_event *__events,
		       int __maxevents, int __timeout)
	__attr_access ((__write_only__, 2, 3)) __nonnull ((2));
```



```cpp
/* Return file descriptor for generic event channel.  Set initial
   value to COUNT.  */
extern int eventfd (unsigned int __count, int __flags) __THROW;
```



```cpp
/* Write N bytes of BUF to FD.  Return the number written, or -1.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern ssize_t write (int __fd, const void *__buf, size_t __n) __wur
    __attr_access ((__read_only__, 2, 3));
```



```cpp
/* Read NBYTES into BUF from FD.  Return the
   number read, -1 for errors or 0 for EOF.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern ssize_t read (int __fd, void *__buf, size_t __nbytes) __wur
    __fortified_attr_access (__write_only__, 2, 3);
```



EINTR 表示这次read 被信号打断了，重试即可；EAGAIN 表示在非阻塞模式下当前已经没有可读计数了



```cpp
/* Await a connection on socket FD.
   When a connection arrives, open a new socket to communicate with it,
   set *ADDR (which is *ADDR_LEN bytes long) to the address of the connecting
   peer and *ADDR_LEN to the address's actual length, and return the
   new socket's descriptor, or -1 for errors.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int accept (int __fd, __SOCKADDR_ARG __addr,
		   socklen_t *__restrict __addr_len);

#ifdef __USE_GNU
/* Similar to 'accept' but takes an additional parameter to specify flags.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int accept4 (int __fd, __SOCKADDR_ARG __addr,
		    socklen_t *__restrict __addr_len, int __flags);
#endif
```

在非阻塞 socket 上调用 accept 时，如果当前没有可立即取出的新连接，系统不会像阻塞模式那样卡住等待，而是直接返回 -1，并把 errno 设成 EAGAIN 或 EWOULDBLOCK；这在语义上并不表示“真正出错了”，而是表示“现在暂时没有资源可取，稍后再试”



```cpp
/* Set socket FD's option OPTNAME at protocol level LEVEL
   to *OPTVAL (which is OPTLEN bytes long).
   Returns 0 on success, -1 for errors.  */
#ifndef __USE_TIME_BITS64
extern int setsockopt (int __fd, int __level, int __optname,
		       const void *__optval, socklen_t __optlen) __THROW;
```

```cpp
::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
```

放到你这句里逐个看：listen_fd 是要设置选项的那个 socket 文件描述符；SOL_SOCKET 表示“这个选项属于 socket 通用层”，不是 TCP 专属层、也不是IP 专属层；SO_REUSEADDR 是具体要设置的选项名，表示允许重用本地地址，开发时对服务器重启特别常见；&optval 是选项值的地址，你这里通常传一个 int optval = 1 表示打开，传 0 表示关闭；sizeof(optval) 就是这个值占多少字节，让内核知道该从用户态拷贝多大一块数据。换句话说，这句代码的自然语言就是：“对listen_fd 这个 socket，在 socket 通用层上，把 SO_REUSEADDR 这个选项设置成开启”。
