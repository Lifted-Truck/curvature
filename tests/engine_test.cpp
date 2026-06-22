#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <thread>
#include <vector>

#include "../src/dsp/ModalBank.h"
#include "../src/engine/SpectrumBus.h"
#include "../src/engine/StrikeQueue.h"
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

TEST_CASE("harmonic morph snaps partials to the harmonic series")
{
    // at harmonic = 1, an inharmonic object's partials should land on integer
    // multiples of the fundamental (a pitched/harmonic tone)
    SpectrumFrame f;
    f.numModes = 24;
    for (int m = 0; m < 24; ++m) {            // deliberately inharmonic ratios
        f.ratio[m] = 1.0f + 1.0f * m + 0.37f * std::sin((float) m);
        f.coupling[m] = 1.0f / (m + 1);
    }
    ModalVoice voice;
    voice.prepare(48000.0);
    voice.setHarmonic(1.0f);
    voice.noteOn(f, 69, 1.0f, 200.0f, 12000.0f, { 4.0f, 0.5f, 0.3f }, 0);
    // can't read freqs directly; instead verify via a long render's FFT peaks
    std::vector<float> buf(48000 * 2, 0.0f);
    for (size_t i = 0; i < buf.size(); i += 512)
        voice.renderAdd(buf.data() + i, (int) std::min<size_t>(512, buf.size() - i));

    const int N = 1 << 16;
    std::vector<float> win(N);
    for (int i = 0; i < N; ++i) win[i] = buf[(size_t) i] * (0.5f - 0.5f * std::cos(2.0f * (float) M_PI * i / N));
    // crude DFT at the first few harmonic bins of f0=200 Hz: energy should
    // concentrate near integer multiples
    auto binEnergy = [&](float hz) {
        const float wgt = 2.0f * (float) M_PI * hz / 48000.0f;
        float re = 0, im = 0;
        for (int i = 0; i < N; ++i) { re += win[i] * std::cos(wgt * i); im += win[i] * std::sin(wgt * i); }
        return std::sqrt(re * re + im * im);
    };
    // harmonic energy (on 200 Hz grid) should beat the energy at off-harmonic
    // points (e.g. 1.5x, 2.5x) -> the spectrum is now harmonic
    const float onGrid = binEnergy(200) + binEnergy(400) + binEnergy(600) + binEnergy(800);
    const float offGrid = binEnergy(300) + binEnergy(500) + binEnergy(700) + binEnergy(900);
    REQUIRE(onGrid > 2.0f * offGrid);
}

TEST_CASE("bowed voice: sustains energy and stays bounded (servo can't blow up)")
{
    ModalVoice voice;
    voice.prepare(48000.0);
    voice.setBow(0.9f);
    voice.setImpulse(0.0f);  // pure bow
    voice.noteOn(testFrame(64), 60, 1.0f, 110.0f, 6000.0f, { 4.0f, 0.5f, 0.3f }, 0);

    std::vector<float> buf(48000 * 6, 0.0f);
    for (size_t i = 0; i < buf.size(); i += 512)
        voice.renderAdd(buf.data() + i, (int) std::min<size_t>(512, buf.size() - i));

    // a short bow stroke must already speak (fast-succession notes were silent
    // when the swell was a slow 0.6 s linear ramp — impulse only)
    float earlyRms = 0.0f;
    for (int i = 4800; i < 4800 + 4800; ++i)  // ~0.1-0.2 s window
        earlyRms += buf[i] * buf[i];
    REQUIRE(std::sqrt(earlyRms / 4800.0f) > 0.005f);

    float peak = 0.0f, lateRms = 0.0f;
    const size_t lateStart = buf.size() - 48000;  // last second
    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE(std::isfinite(buf[i]));
        peak = std::max(peak, std::abs(buf[i]));
        if (i >= lateStart)
            lateRms += buf[i] * buf[i];
    }
    lateRms = std::sqrt(lateRms / 48000.0f);
    REQUIRE(peak < 128.0f);     // amplitude servo stays bounded
    REQUIRE(lateRms > 0.01f);   // and the bow actually sustains (no decay to silence)
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

TEST_CASE("tap-tap-tap then hold still sounds (note-trigger regression)")
{
    // Julian: tapping a few times then holding produced silence. Mirror the
    // plugin's block loop (render in 512-sample blocks, updateActivity each
    // block) with bow + impulse both on.
    VoiceManager vm;
    vm.prepare(48000.0);
    vm.setDamping({ 5.0f, 0.7f, 0.3f, 0.0f, 0.5f });
    vm.setBow(0.7f);
    vm.setImpulse(1.0f);
    auto frame = testFrame();
    vm.setGlobalFrame(&frame);  // default Voices = Global Flow

    std::vector<float> buf(2048);
    float phase = 0.0f;
    auto render = [&](int blocks) {
        for (int b = 0; b < blocks; ++b) {
            phase += 0.05f;  // mutate the frame like morph does
            for (int m = 0; m < frame.numModes; ++m)
                frame.ratio[m] = std::sqrt((float) (m + 1)) * (1.0f + 0.1f * std::sin(phase + m));
            vm.setGlobalFrame(&frame);
            std::fill(buf.begin(), buf.end(), 0.0f);
            vm.renderAdd(buf.data(), (int) buf.size());
            vm.updateActivity();
        }
    };

    for (int tap = 0; tap < 12; ++tap) {  // more taps than voices -> stealing
        vm.noteOn(frame, 48 + tap, 0.9f, 4000.0f);
        render(1);
        vm.noteOff(48 + tap);
        render(1);
    }

    vm.noteOn(frame, 60, 0.9f, 4000.0f);   // now HOLD it
    float rms = 0.0f;
    int count = 0;
    for (int b = 0; b < 20; ++b) {
        std::fill(buf.begin(), buf.end(), 0.0f);
        vm.renderAdd(buf.data(), (int) buf.size());
        vm.updateActivity();
        for (float x : buf) { rms += x * x; ++count; }
    }
    REQUIRE(std::sqrt(rms / count) > 0.01f);  // the held note must sound
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

TEST_CASE("strike queue: SPSC delivers every event in order, drops when full")
{
    StrikeQueue<8> q;
    StrikeEvent e;
    REQUIRE_FALSE(q.pop(e));  // empty

    for (int i = 0; i < 8; ++i)
        REQUIRE(q.push({ (float) i, 1.0f }));
    REQUIRE_FALSE(q.push({ 99.0f, 1.0f }));  // full -> dropped, never blocks

    for (int i = 0; i < 8; ++i) {
        REQUIRE(q.pop(e));
        REQUIRE(e.strikeParam == (float) i);  // FIFO order preserved
    }
    REQUIRE_FALSE(q.pop(e));
}

TEST_CASE("strike queue: threaded producer/consumer loses nothing")
{
    StrikeQueue<64> q;
    std::atomic<bool> done { false };
    constexpr int N = 100000;

    std::thread producer([&] {
        for (int i = 0; i < N; ) {
            if (q.push({ (float) i, 1.0f }))
                ++i;  // retry on full (consumer will catch up)
        }
        done = true;
    });

    int received = 0;
    StrikeEvent e;
    float expected = 0.0f;
    while (!done.load() || received < N) {
        if (q.pop(e)) {
            REQUIRE(e.strikeParam == expected);  // strict FIFO, nothing lost
            expected += 1.0f;
            ++received;
        }
    }
    producer.join();
    REQUIRE(received == N);
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
