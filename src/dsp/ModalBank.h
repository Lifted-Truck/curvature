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
    float comb = 0.0f;         // 0..1: every other mode's T60 collapses —
                               // selective absorption no material has
};

class ModalVoice {
public:
    void prepare(double sampleRate) { sr_ = sampleRate; reset(); }

    void reset()
    {
        numModes_ = 0;
        active_ = false;
        globalFrame_ = nullptr;
        for (int m = 0; m < kMaxModes; ++m)
            s_[m] = c_[m] = exc_[m] = 0.0f;
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
        noteHz_ = noteHz;
        startOrder_ = order;
        releasing_ = false;
        damping_ = damping;
        rng_ = 0x9e3779b9u ^ (order * 2654435761u);
        bowAge_ = 0;

        // bow is an alternative excitation, not a layer: it crossfades the
        // mallet impulse out and swells in as sustained energy instead
        const float impulseGain = 1.0f - 0.9f * bow_;

        const float nyquistGuard = static_cast<float>(0.45 * sr_);
        int k = 0;
        for (int m = 0; m < frame.numModes && k < kMaxModes; ++m) {
            const float f = noteHz * frame.ratio[m];
            if (f >= nyquistGuard)
                break;  // ratios ascend; everything after is out too
            freq_[k] = f;
            const float mallet = 1.0f / (1.0f + std::pow(f / malletCutoff, 4.0f));
            exc_[k] = velocity * frame.coupling[m] * mallet;
            s_[k] += exc_[k] * impulseGain;
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

    // global-flow mode: retune ringing modes to the evolving spectrum.
    // 'frame' must stay valid for the current audio block only.
    void setGlobalTuning(const SpectrumFrame* frame) { globalFrame_ = frame; }

    // sustained stochastic excitation (crude bow; refined by ear Phase 2+)
    void setBow(float amount) { bow_ = amount; }

    // pitch wheel: multiplicative factor on all mode frequencies
    void setPitchBend(float factor) { bend_ = factor; }

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
        // global-flow retune: pull mode frequencies from the live spectrum
        // before computing rotation targets; the linear per-sample ramps
        // below carry the sweep (coupled form stays stable under it)
        if (globalFrame_ != nullptr) {
            const float nyquistGuard = static_cast<float>(0.45 * sr_);
            const int k = std::min(numModes_, globalFrame_->numModes);
            for (int m = 0; m < k; ++m)
                freq_[m] = std::min(noteHz_ * globalFrame_->ratio[m], nyquistGuard);
            f1_ = freq_[0];
        }

        // bow: one stochastic energy packet per control interval, weighted
        // by the strike coupling pattern (the bow excites what the strike
        // point couples to), swelling in over ~0.6 s so the note has a
        // bowed attack rather than a struck one
        if (bow_ > 0.0f && !releasing_) {
            ++bowAge_;
            const float swell = std::min(1.0f,
                (float) bowAge_ * kControlInterval / (0.6f * (float) sr_));
            const float g = bow_ * bow_ * 0.03f * swell;
            for (int m = 0; m < numModes_; ++m) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float noise = (float) (int32_t) rng_ * 4.6566e-10f;  // ~[-1,1]
                s_[m] += g * exc_[m] * noise;
            }
        }

        const float lnK = std::log(1000.0f);
        for (int m = 0; m < numModes_; ++m) {
            float t60 = releasing_
                ? std::min(damping_.releaseT60, damping_.t60)
                : damping_.t60 * std::pow(f1_ / freq_[m], damping_.tilt);
            if ((m & 1) != 0)
                t60 *= 1.0f - 0.97f * damping_.comb;
            t60 = std::max(t60, 1e-3f);
            const float r = std::exp(-lnK / (t60 * static_cast<float>(sr_)));
            const float w = 2.0f * static_cast<float>(M_PI)
                            * std::min(freq_[m] * bend_, static_cast<float>(0.45 * sr_))
                            / static_cast<float>(sr_);
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
    float noteHz_ = 440.0f;
    uint32_t startOrder_ = 0;
    bool active_ = false;
    bool releasing_ = false;
    int samplesUntilControl_ = 0;
    float f1_ = 1.0f;
    float bow_ = 0.0f;
    float bend_ = 1.0f;
    int bowAge_ = 0;
    uint32_t rng_ = 1;
    const SpectrumFrame* globalFrame_ = nullptr;
    DampingParams damping_;

    float freq_[kMaxModes] = {};
    float exc_[kMaxModes] = {};
    float s_[kMaxModes] = {};
    float c_[kMaxModes] = {};
    float rc_[kMaxModes] = {};
    float rs_[kMaxModes] = {};
    float rcStep_[kMaxModes] = {};
    float rsStep_[kMaxModes] = {};
};

} // namespace curv
