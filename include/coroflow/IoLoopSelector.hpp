#pragma once

#include <cstddef>

namespace coroflow {
// 当前只实现 round-robin 后续可以根据每个 EventLoop 的实际情况动态调整
class IoLoopSelector {
public:
    explicit IoLoopSelector(size_t loopNums)
        :loopNums_(loopNums){}
    ~IoLoopSelector() =default;

    size_t next() {
        return (++curIndex_) % loopNums_;
    }
private:
    size_t curIndex_ {0};
    size_t loopNums_;
};
}