// Wait-free single-producer/single-consumer triple buffer carrying the
// latest SpectrumFrame from the geometry thread to the audio thread.
//
// Audio-thread rules (CLAUDE.md): readLatest() does no allocation, takes no
// locks, never blocks. If the producer has published nothing new, the
// consumer keeps its current frame — audio coasts on the last spectrum.
#pragma once

#include <atomic>

#include "SpectrumFrame.h"

namespace curv {

class SpectrumBus {
public:
    // producer (geometry thread): fill a slot, then publish it
    SpectrumFrame& beginWrite() { return slots_[writeIdx_]; }

    void publish()
    {
        // swap the written slot into 'ready'; reuse whatever was there
        writeIdx_ = ready_.exchange(writeIdx_ | kFreshBit, std::memory_order_acq_rel) & kIdxMask;
        hasEverPublished_.store(true, std::memory_order_release);
    }

    // consumer (audio thread): returns true if 'frame' index was updated to
    // a fresh slot; the previously returned slot stays valid until the call
    // after next (triple buffering), so callers must copy what they keep.
    bool readLatest(const SpectrumFrame** frame)
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

    SpectrumFrame slots_[3];
    int writeIdx_ = 0;
    int readIdx_ = 1;
    std::atomic<int> ready_ { 2 };
    std::atomic<bool> hasEverPublished_ { false };
};

} // namespace curv
