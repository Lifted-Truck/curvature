// Fixed-size polyphony with oldest-voice stealing. Each voice snapshots the
// current SpectrumFrame at note-on (Phase 1 static geometry == snapshot
// mode; global-flow mode arrives with Phase 2). No allocation anywhere.
#pragma once

#include <cmath>

#include "../dsp/ModalBank.h"
#include "SpectrumFrame.h"

namespace curv {

constexpr int kNumVoices = 8;

class VoiceManager {
public:
    void prepare(double sampleRate)
    {
        for (auto& v : voices_)
            v.prepare(sampleRate);
        order_ = 0;
    }

    void setDamping(const DampingParams& d)
    {
        damping_ = d;
        for (auto& v : voices_)
            v.setDamping(d);
    }

    // nullptr = snapshot mode; non-null = all voices track the live spectrum
    // (pointer must remain valid for the current audio block)
    void setGlobalFrame(const SpectrumFrame* frame)
    {
        for (auto& v : voices_)
            v.setGlobalTuning(frame);
    }

    void setBow(float amount)
    {
        for (auto& v : voices_)
            v.setBow(amount);
    }

    void setPitchBend(float factor)
    {
        for (auto& v : voices_)
            v.setPitchBend(factor);
    }

    void noteOn(const SpectrumFrame& frame, int midiNote, float velocity, float malletCutoff)
    {
        ModalVoice* slot = nullptr;
        for (auto& v : voices_)
            if (!v.isActive()) { slot = &v; break; }
        if (slot == nullptr) {  // steal: oldest, preferring releasing voices
            uint32_t best = UINT32_MAX;
            for (auto& v : voices_) {
                // releasing voices steal first, then oldest
                const uint32_t score = v.startOrder() + (v.isReleasing() ? 0u : 0x40000000u);
                if (score < best) { best = score; slot = &v; }
            }
            slot->reset();
        }
        const float hz = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
        slot->noteOn(frame, midiNote, velocity, hz, malletCutoff, damping_, order_++);
    }

    void noteOff(int midiNote)
    {
        for (auto& v : voices_)
            if (v.isActive() && v.noteNumber() == midiNote && !v.isReleasing())
                v.noteOff();
    }

    void allNotesOff()
    {
        for (auto& v : voices_)
            v.reset();
    }

    void renderAdd(float* out, int n)
    {
        for (auto& v : voices_)
            v.renderAdd(out, n);
    }

    void updateActivity()
    {
        for (auto& v : voices_)
            v.updateActivity();
    }

private:
    ModalVoice voices_[kNumVoices];
    DampingParams damping_;
    uint32_t order_ = 0;
};

} // namespace curv
