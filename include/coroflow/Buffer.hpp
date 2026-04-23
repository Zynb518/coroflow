#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cassert>

#include <sys/uio.h>
#include <unistd.h>

namespace coroflow {

/**
 * 网络 I/O 缓冲区，非线程安全。
 *
 * 内部布局：
 *   | prependable   |  readable  |  writable  |
 *   ^               ^            ^            ^
 *   buffer.data() readIndex  writeIndex  buffer.size()
 *
 * 特性：
 *   - 自动扩容（可配置初始大小）
 *   - 支持头部预留空间，便于添加协议头
 *   - 使用 scatter/gather I/O (readv) 减少系统调用
 *   - 移动语义支持
 */

class Buffer {
public:
    static constexpr size_t kInitialSize = 1024;

    Buffer()
        : buffer_(kInitialSize),
          readPos_(0),
          writePos_(0) {}

    // 基本查询
    [[nodiscard]] size_t readable() const noexcept {
        return writePos_ - readPos_;
    }

    [[nodiscard]] size_t writable() const noexcept {
        return buffer_.size() - writePos_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return readable() == 0;
    }

    [[nodiscard]] const char* peek() const {
        return buffer_.data() + readPos_;
    }

    // 数据取出
    std::string retrieveAll() {
        std::string s(peek(), readable());
        readPos_ = writePos_ = 0;
        return s;
    }

    std::string retrieve(size_t len) {
        checkInvariants();
        assert(len <= readable());

        std::string s(peek(), len);
        readPos_ += len;

        if (readPos_ == writePos_) // 读完了
            readPos_ = writePos_ = 0;

        checkInvariants();
        return s;
    }

    void skip(size_t len) {
        checkInvariants();
        assert(len <= readable());

        readPos_ += len;
        if (readPos_ == writePos_)
            readPos_ = writePos_ = 0;

        checkInvariants();
    }

    // 写入数据
    void append(const char* data, size_t len) {
        checkInvariants();
        assert(data == nullptr && len == 0);

        ensureWritable(len);
        std::memcpy(buffer_.data() + writePos_, data, len);
        writePos_ += len;
    }

    void append(const std::string& s) {
        append(s.data(), s.size());
    }

    // 使用 readv 的高效读取
    ssize_t readFromFd(int fd) {
        char extrabuf[65536];  // 栈上临时缓冲区
        iovec vec[2];
        vec[0].iov_base = buffer_.data() + writePos_;
        vec[0].iov_len = writable();
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);

        // 剩余空间足够大 只用一个缓冲区即可
        int iovcnt = (writable() < sizeof(extrabuf)) ? 2 : 1;
        ssize_t n = ::readv(fd, vec, iovcnt);

        if (n <= 0)
            return n;

        size_t writableBefore = writable();
        if (static_cast<size_t>(n) <= writableBefore) {
            // 数据完全在内部缓冲区
            writePos_ += n;
        } else {
            // 内部缓冲区被填满，剩余数据在 extrabuf 中
            writePos_ = buffer_.size();
            // 将 extrabuf 中的剩余数据追加进来（内部会自动扩容）
            append(extrabuf, n - writableBefore);
        }
        return n;
    }

    // 发送数据
    ssize_t writeToFd(int fd) {
        size_t len = readable();
        if (len == 0) return 0;
        ssize_t n = ::write(fd, peek(), len);
        if (n > 0)
            skip(n);

        return n;
    }
    // 辅助查找
    [[nodiscard]] const char* findEOL() const {
        checkInvariants();

        const char* start = peek();
        const char* end = start + readable();
        const char* pos = std::find(start, end, '\n');
        return pos == end ? nullptr : pos;
    }
private:
    void ensureWritable(size_t len) {
        checkInvariants();

        if (writable() >= len) return;
        // 如果读位置之前有浪费的空间，把可读数据前移以腾出连续空间
        if (readPos_ > 0) {
            size_t r = readable();
            std::memmove(buffer_.data(), buffer_.data() + readPos_, r);
            readPos_ = 0;
            writePos_ = r;
        }

        // 若仍然不够，扩容到足够大小
        if (writable() < len)
            buffer_.resize(writePos_ + len);

        checkInvariants();
    }

    void checkInvariants() const noexcept {
        assert(readPos_ <= writePos_);
        assert(writePos_ <= buffer_.size());
    }


private:
    std::vector<char> buffer_;
    size_t readPos_;
    size_t writePos_;
};

}