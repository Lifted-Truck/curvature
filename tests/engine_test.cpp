#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <thread>
#include <vector>

#include "../src/dsp/ModalBank.h"
#include "../src/engine/SpectrumBus.h"
#include "../src/engine/VoiceManager.h"

using namespace curv;

namespace {

SpectrumFrame testFrame(int k = 32)
{
    SpectrumFrame f;
    f.numModes = k;
    for (int m = 0; m < k; ++m) {
        f.ratio[m] = std::sqrt((float) (m + 1));
        f.coupling[m] = 1.0f / (float) (m + 1);
    }
    return f;
}

} // namespace

TEST_CASE("modal voice: bounded, NaN-free, decays to silence")
{
    ModalVoice voice;
    voice.prepare(48000.0);
    voice.noteOn(testFrame(), 69, 1.0f, 440.0f, 8000.0f, { 0.5f, 0.7f, 0.2f }, 0);

    std::vector<float> buf(48000 * 4, 0.0f);
    for (size_t i = 0; i < buf.size(); i += 512) {
        voice.renderAdd(buf.data() + i, (int) std::min<size_t>(512, buf.size() - i));
        voice.updateActivity();
    }
    float peak = 0.0f, tailPeak = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE(std::isfinite(buf[i]));
        peak = std::max(peak, std::abs(buf[i]));
        if (i > buf.size() - 4800)
            tailPeak = std::max(tailPeak, std::abs(buf[i]));
    }
    REQUIRE(peak > 0.01f);     // it actually rang
    REQUIRE(peak < 64.0f);     // and stayed bounded
    REQUIRE(tailPeak < 1e-3f); // T60 0.5s: silent after 4s
}

TEST_CASE("modal voice: stable under continuous damping parameter sweeps")
{
    ModalVoice voice;
    voice.prepare(48000.0);
    voice.noteOn(testFrame(64), 69, 1.0f, 220.0f, 12000.0f, { 8.0f, 0.0f, 0.3f }, 0);

    std::vector<float> buf(512);
    for (int block = 0; block < 400; ++block) {
        // sweep tilt across its whole range, including negative, mid-note
        const float tilt = -1.0f + 3.0f * (block % 100) / 100.0f;
        voice.setDamping({ 8.0f, tilt, 0.3f });
        std::fill(buf.begin(), buf.end(), 0.0f);
        voice.renderAdd(buf.data(), (int) buf.size());
        for (float x : buf)
            REQUIRE(std::isfinite(x));
    }
}

TEST_CASE("voice manager: polyphony, stealing, note-off")
{
    VoiceManager vm;
    vm.prepare(48000.0);
    vm.setDamping({ 5.0f, 0.7f, 0.2f });

    const auto frame = testFrame();
    for (int i = 0; i < kNumVoices + 4; ++i)  // exceed polyphony: must steal, not crash
        vm.noteOn(frame, 40 + i, 0.8f, 4000.0f);

    std::vector<float> buf(4096, 0.0f);
    vm.renderAdd(buf.data(), (int) buf.size());
    float peak = 0.0f;
    for (float x : buf) {
        REQUIRE(std::isfinite(x));
        peak = std::max(peak, std::abs(x));
    }
    REQUIRE(peak > 0.0f);

    for (int i = 0; i < kNumVoices + 4; ++i)
        vm.noteOff(40 + i);
    vm.allNotesOff();
    std::fill(buf.begin(), buf.end(), 0.0f);
    vm.renderAdd(buf.data(), (int) buf.size());
    for (float x : buf)
        REQUIRE(x == 0.0f);
}

TEST_CASE("spectrum bus: SPSC stress, consumer always sees a coherent frame")
{
    SpectrumBus bus;
    std::atomic<bool> stop { false };

    std::thread producer([&] {
        uint32_t id = 1;
        while (!stop.load()) {
            auto& f = bus.beginWrite();
            f.numModes = 8;
            f.frameId = id;
            // tag every slot value with the frame id to detect torn reads
            for (int m = 0; m < 8; ++m)
                f.ratio[m] = (float) id;
            bus.publish();
            ++id;
        }
    });

    const SpectrumFrame* frame = nullptr;
    uint32_t lastSeen = 0;
    for (int i = 0; i < 200000; ++i) {
        bus.readLatest(&frame);
        if (frame != nullptr) {
            REQUIRE(frame->frameId >= lastSeen);  // monotonic
            for (int m = 0; m < frame->numModes; ++m)
                REQUIRE(frame->ratio[m] == (float) frame->frameId);  // untorn
            lastSeen = frame->frameId;
        }
    }
    stop = true;
    producer.join();
    REQUIRE(lastSeen > 0);
}
