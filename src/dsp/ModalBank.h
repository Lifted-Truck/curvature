// K coupled-form (rotation) resonators — one voice of the modal bank.
//
// Each mode is a 2D rotation (s, c) by angle w = 2*pi*f/sr with radius decay
// r per sample: stable under continuous frequency sweeps (the whole Phase 2
// modulation story), unlike direct-form biquads. Coefficients ramp linearly
// across each control interval (no zipper). Header-only, fixed-size state,
// nothing here allocates after construction — audio-thread safe by layout.
#pragma once

#include <cmath>

#include "../engine/SpectrumFrame.h"

namespace curv {

constexpr int kControlInterval = 64;  // samples per coefficient update

struct DampingParams {
    float t60 = 5.0f;          // seconds, global
    float tilt = 0.7f;         // decay_k = t60 * (f_1/f_k)^tilt; may be negative
    float releaseT60 = 0.25f;  // after note-off
};

class ModalVoice {
public:
    void prepare(double sampleRate) { sr_ = sampleRate; reset(); }

    void reset()
    {
        numModes_ = 0;
        active_ = false;
        for (int m = 0; m < kMaxModes; ++m)
            s_[m] = c_[m] = 0.0f;
    }

    bool isActive() const { return active_; }
    int noteNumber() const { return note_; }
    uint32_t startOrder() const { return startOrder_; }
    bool isReleasing() const { return releasing_; }

    void noteOn(const SpectrumFrame& frame, int midiNote, float velocity,
                float noteHz, float malletCutoff, const DampingParams& damping,
                uint32_t order)
    {
        note_ = midiNote;
        startOrder_ = order;
        releasing_ = false;
        damping_ = damping;

        const float nyquistGuard = static_cast<float>(0.45 * sr_);
        int k = 0;
        for (int m = 0; m < frame.numModes && k < kMaxModes; ++m) {
            const float f = noteHz * frame.ratio[m];
            if (f >= nyquistGuard)
                break;  // ratios ascend; everything after is out too
            freq_[k] = f;
            const float mallet = 1.0f / (1.0f + std::pow(f / malletCutoff, 4.0f));
            const float amp = velocity * frame.coupling[m] * mallet;
            s_[k] += amp;  // impulse injection into the sine state
            c_[k] = c_[k]; // cosine state untouched
            k++;
        }
        numModes_ = k;
        f1_ = freq_[0];
        updateCoefficients(true);
        active_ = true;
        samplesUntilControl_ = 0;
    }

    void noteOff() { releasing_ = true; samplesUntilControl_ = 0; }

    void setDamping(const DampingParams& damping) { damping_ = damping; }

    // add this voice into out[0..n)
    void renderAdd(float* out, int n)
    {
        if (!active_)
            return;
        int i = 0;
        while (i < n) {
            if (samplesUntilControl_ <= 0)
                updateCoefficients(false);
            const int run = std::min(n - i, samplesUntilControl_);
            for (int j = 0; j < run; ++j) {
                float sum = 0.0f;
                for (int m = 0; m < numModes_; ++m) {
                    // ramped coefficients: linear step per sample
                    rc_[m] += rcStep_[m];
                    rs_[m] += rsStep_[m];
                    const float s0 = s_[m], c0 = c_[m];
                    s_[m] = s0 * rc_[m] + c0 * rs_[m];
                    c_[m] = c0 * rc_[m] - s0 * rs_[m];
                    sum += s_[m];
                }
                out[i + j] += sum;
            }
            i += run;
            samplesUntilControl_ -= run;
        }
    }

    // call at block rate from the audio thread; cheap energy gate
    void updateActivity()
    {
        if (!active_)
            return;
        float energy = 0.0f;
        for (int m = 0; m < numModes_; ++m)
            energy += s_[m] * s_[m] + c_[m] * c_[m];
        if (energy < 1e-10f)
            reset();
    }

private:
    void updateCoefficients(bool immediate)
    {
        const float lnK = std::log(1000.0f);
        for (int m = 0; m < numModes_; ++m) {
            float t60 = releasing_
                ? std::min(damping_.releaseT60, damping_.t60)
                : damping_.t60 * std::pow(f1_ / freq_[m], damping_.tilt);
            t60 = std::max(t60, 1e-3f);
            const float r = std::exp(-lnK / (t60 * static_cast<float>(sr_)));
            const float w = 2.0f * static_cast<float>(M_PI) * freq_[m] / static_cast<float>(sr_);
            const float targetRc = r * std::cos(w);
            const float targetRs = r * std::sin(w);
            if (immediate) {
                rc_[m] = targetRc;
                rs_[m] = targetRs;
                rcStep_[m] = rsStep_[m] = 0.0f;
            } else {
                rcStep_[m] = (targetRc - rc_[m]) / kControlInterval;
                rsStep_[m] = (targetRs - rs_[m]) / kControlInterval;
            }
        }
        samplesUntilControl_ = kControlInterval;
    }

    double sr_ = 48000.0;
    int numModes_ = 0;
    int note_ = -1;
    uint32_t startOrder_ = 0;
    bool active_ = false;
    bool releasing_ = false;
    int samplesUntilControl_ = 0;
    float f1_ = 1.0f;
    DampingParams damping_;

    float freq_[kMaxModes] = {};
    float s_[kMaxModes] = {};
    float c_[kMaxModes] = {};
    float rc_[kMaxModes] = {};
    float rs_[kMaxModes] = {};
    float rcStep_[kMaxModes] = {};
    float rsStep_[kMaxModes] = {};
};

} // namespace curv
