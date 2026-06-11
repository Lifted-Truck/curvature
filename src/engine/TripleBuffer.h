// Wait-free single-producer/single-consumer triple buffer. The consumer's
// readLatest never blocks and never allocates; if nothing fresh is
// published it keeps the last frame (audio coasts; UI holds still).
#pragma once

#include <atomic>

namespace curv {

template <typename Frame>
class TripleBuffer {
public:
    Frame& beginWrite() { return slots_[writeIdx_]; }

    void publish()
    {
        writeIdx_ = ready_.exchange(writeIdx_ | kFreshBit, std::memory_order_acq_rel) & kIdxMask;
        hasEverPublished_.store(true, std::memory_order_release);
    }

    // returns true if 'frame' now points at a fresh slot; the previously
    // returned slot stays valid until the call after next — copy what you keep
    bool readLatest(const Frame** frame)
    {
        const int ready = ready_.load(std::memory_order_acquire);
        if ((ready & kFreshBit) == 0) {
            *frame = hasEverPublished_.load(std::memory_order_acquire) ? &slots_[readIdx_] : nullptr;
            return false;
        }
        readIdx_ = ready_.exchange(readIdx_, std::memory_order_acq_rel) & kIdxMask;
        *frame = &slots_[readIdx_];
        return true;
    }

private:
    static constexpr int kFreshBit = 4;
    static constexpr int kIdxMask = 3;

    Frame slots_[3];
    int writeIdx_ = 0;
    int readIdx_ = 1;
    std::atomic<int> ready_ { 2 };
    std::atomic<bool> hasEverPublished_ { false };
};

} // namespace curv
