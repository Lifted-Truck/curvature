// Lock-free SPSC queue carrying strike events from the audio thread (note-on)
// to the geometry thread, which dents the manifold at the strike point in
// response — the performance reshaping the instrument. Audio-thread safe:
// fixed storage, no locks, no allocation; a full queue drops the event
// (never blocks the audio thread).
#pragma once

#include <atomic>
#include <cstdint>

namespace curv {

struct StrikeEvent {
    float strikeParam = 0.0f;  // where on the surface (global strike param at note-on)
    float velocity = 0.0f;     // how hard
};

template <uint32_t CapacityPow2 = 64>
class StrikeQueue {
public:
    // producer (audio thread): returns false if full (event dropped)
    bool push(const StrikeEvent& e)
    {
        const uint32_t h = head_.load(std::memory_order_relaxed);
        const uint32_t t = tail_.load(std::memory_order_acquire);
        if (h - t >= CapacityPow2)
            return false;
        buf_[h & kMask] = e;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // consumer (geometry thread)
    bool pop(StrikeEvent& out)
    {
        const uint32_t t = tail_.load(std::memory_order_relaxed);
        const uint32_t h = head_.load(std::memory_order_acquire);
        if (t == h)
            return false;
        out = buf_[t & kMask];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

private:
    static constexpr uint32_t kMask = CapacityPow2 - 1;
    static_assert((CapacityPow2 & kMask) == 0, "capacity must be a power of two");

    StrikeEvent buf_[CapacityPow2];
    std::atomic<uint32_t> head_ { 0 };
    std::atomic<uint32_t> tail_ { 0 };
};

} // namespace curv
