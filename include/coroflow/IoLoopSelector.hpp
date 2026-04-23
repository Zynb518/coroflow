#pragma once

#include <cstddef>

namespace coroflow {
// 当前只实现 round-robin 后续可以根据每个 EventLoop 的实际情况动态调整
class IoLoopSelector {
public:
    explicit IoLoopSelector(size_t loopNums)
        :loopNums(loopNums){}
    ~IoLoopSelector() =default;

    size_t next() {
        return (++curIndex) % loopNums;
    }
private:
    size_t curIndex {0};
    size_t loopNums;
};
}