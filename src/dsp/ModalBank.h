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
    float comb = 0.0f;         // 0..1: notch depth — selective per-mode
                               // absorption no material has (extreme at top)
    float combFreq = 0.5f;     // 0..1: notch spacing over mode index
                               // (low = wide notches, high = every other mode)
};

class ModalVoice {
public:
    void prepare(double sampleRate) { sr_ = sampleRate; reset(); }

    void reset()
    {
        numModes_ = 0;
        active_ = false;
        globalFrame_ = nullptr;
        injectPos_ = injectLen_ = 0;
        bowAge_ = 0;
        bowNoise_ = 0.0f;
        for (int m = 0; m < kMaxModes; ++m)
            s_[m] = c_[m] = exc_[m] = bowWeight_[m] = bowTarget_[m] = ratio_[m] = 0.0f;
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

        // impulse (mallet) and bow are independent excitation levels:
        // impulse 1 / bow 0 = struck, impulse 0 / bow 1 = purely bowed,
        // both up = struck note with sustained bowing underneath
        impulseAmp_ = impulse_;

        // mallet contact time: energy enters over a raised-cosine window,
        // not in one sample (a true impulse clicks). Harder mallet (higher
        // cutoff) = shorter contact.
        const float contactMs = std::clamp(1800.0f / malletCutoff, 0.3f, 6.0f);
        injectLen_ = std::max(16, (int) (contactMs * 0.001f * sr_));
        injectPos_ = 0;

        const float nyquistGuard = static_cast<float>(0.45 * sr_);
        int k = 0;
        for (int m = 0; m < frame.numModes && k < kMaxModes; ++m) {
            // warp (dispersion) + harmonic snap; see applySpectralTransform
            const float f = noteHz * applySpectralTransform(frame.ratio[m]);
            if (f >= nyquistGuard)
                continue;  // skip (mode order isn't guaranteed ascending after matching)
            ratio_[k] = frame.ratio[m];  // base (unwarped) ratio for live warp
            freq_[k] = f;
            const float mallet = 1.0f / (1.0f + std::pow(f / malletCutoff, 4.0f));
            exc_[k] = velocity * frame.coupling[m] * mallet;
            k++;
        }
        numModes_ = k;
        f1_ = freq_[0] > 0.0f ? freq_[0] : noteHz;

        // level normalization: summing K random-phase modes grows RMS ~sqrt(K),
        // so brightness/mode-count drove the output into the soft-clip. Scale
        // injection by makeup/sqrt(K) to keep loudness ~constant and leave
        // clean headroom (the crunch threshold was unsatisfyingly low).
        const float norm = 2.6f / std::sqrt((float) std::max(k, 1));
        // bowWeight = spatial coupling only (so bowing a nodal line still
        // silences that mode). The frequency tilt + mallet brightness are
        // applied live into bowTarget_ at control rate, so the Mallet slider
        // shapes the sustained bow's timbre as you turn it.
        bowCutoff_ = malletCutoff;
        for (int m = 0; m < k; ++m) {
            exc_[m] *= norm;
            bowWeight_[m] = frame.coupling[m] * norm;
        }

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

    // live mallet cutoff for the bow timbre (shared exciter brightness)
    void setMallet(float cutoff) { bowCutoff_ = cutoff; }

    // mallet impulse level at note-on (independent of bow)
    void setImpulse(float level) { impulse_ = level; }

    // pitch wheel: multiplicative factor on all mode frequencies
    void setPitchBend(float factor) { bend_ = factor; }

    // spectral warp exponent (1 = physical); read at note-on
    void setWarp(float w) { warp_ = w; }

    // harmonicity: 0 = physical ratios, 1 = partials snapped to the harmonic
    // series (the alien object becomes a pitched/harmonic tone)
    void setHarmonic(float h) { harmonic_ = h; }

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
            const bool bowing = bow_ > 0.0f && !releasing_;
            for (int j = 0; j < run; ++j) {
                if (injectPos_ < injectLen_) {
                    // raised-cosine contact: weights sum to ~1 over the window
                    const float w = impulseAmp_
                        * (1.0f - std::cos(2.0f * (float) M_PI * (injectPos_ + 0.5f) / (float) injectLen_))
                        / (float) injectLen_;
                    for (int m = 0; m < numModes_; ++m)
                        s_[m] += exc_[m] * w;
                    ++injectPos_;
                }
                if (bowing) {
                    // bow = a per-mode amplitude servo that sustains each mode
                    // toward a low-tilted target by pumping energy IN PHASE with
                    // its motion (stick-slip): modes ring as pitched tones, not
                    // hiss. A little lowpassed noise seeds + breathes. Swells in
                    // over ~0.6 s. The factor clamp keeps it bounded.
                    rng_ = rng_ * 1664525u + 1013904223u;
                    const float white = (float) (int32_t) rng_ * 4.6566e-10f;
                    bowNoise_ += kBowLp * (white - bowNoise_);  // ~1.6 kHz one-pole
                    ++bowAge_;
                    // power-curve swell: rises fast early (~57% by 150 ms) so
                    // short / fast-succession notes still speak, then blooms to
                    // full over ~0.6 s for sustained notes. (A linear 0.6 s
                    // ramp left short notes inaudibly bowed — impulse only.)
                    // attack = the swell envelope ramping the TARGET (the servo
                    // grows exponentially, so lowering its rate barely slows the
                    // attack; ramping the target it chases gives a controllable
                    // ~0.6 s bloom that still speaks early for short notes).
                    const float swell = std::pow(
                        std::min(1.0f, (float) bowAge_ / (0.6f * (float) sr_)), 0.6f);
                    // seed through bowTarget_ (which carries the mallet rolloff)
                    // so the Mallet slider shapes the bow: a broadband seed here
                    // would brighten every mode regardless of the cutoff.
                    const float seed = 0.05f * bow_ * swell * bowNoise_;
                    const float k = 0.012f;
                    for (int m = 0; m < numModes_; ++m) {
                        s_[m] += seed * bowTarget_[m];                 // mallet-shaped breath
                        const float st = bowTarget_[m] * swell;
                        const float tgt2 = st * st + 1e-12f;
                        const float amp2 = s_[m] * s_[m] + c_[m] * c_[m];
                        // ADD-ONLY: pump energy when a mode is below its target,
                        // never below 1.0 — the bow must not damp the strike
                        // (that suppressed struck+bowed notes). Natural T60
                        // handles decay; the bow only fills sustain underneath.
                        if (amp2 < tgt2) {
                            const float f = std::min(1.0f + k * (tgt2 - amp2) / tgt2, 1.01f);
                            s_[m] *= f;
                            c_[m] *= f;
                        }
                    }
                }
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

    // call at block rate from the audio thread; cheap energy gate. Also the
    // NaN circuit-breaker: a non-finite state would otherwise stick forever
    // (every later injection adds into NaN), reading as "MIDI stopped".
    void updateActivity()
    {
        if (!active_)
            return;
        float energy = 0.0f;
        for (int m = 0; m < numModes_; ++m)
            energy += s_[m] * s_[m] + c_[m] * c_[m];
        if (energy < 1e-10f || !std::isfinite(energy))
            reset();
    }

private:
    // spectral transform applied to a base partial ratio: warp (dispersion)
    // then harmonic snap (pull toward the nearest integer harmonic of f1).
    float applySpectralTransform(float ratio) const
    {
        float w = warp_ == 1.0f ? ratio : std::pow(ratio, warp_);
        if (harmonic_ > 1e-4f) {
            const float snapped = std::max(1.0f, std::round(w));
            w = (1.0f - harmonic_) * w + harmonic_ * snapped;
        }
        return w;
    }

    void updateCoefficients(bool immediate)
    {
        // Global Flow mode: retune at control rate so the evolving spectrum
        // AND spectral warp are live (sweepable while notes ring). Snapshot
        // mode leaves freq_ frozen at its note-on values, so warp is per-note
        // there — the liveness of warp tracks the Voices switch (Snapshot =
        // the old frozen behavior, Global = live).
        if (globalFrame_ != nullptr) {
            const float nyquistGuard = static_cast<float>(0.45 * sr_);
            const int gk = std::min(numModes_, globalFrame_->numModes);
            for (int m = 0; m < numModes_; ++m) {
                const float base = m < gk ? globalFrame_->ratio[m] : ratio_[m];
                freq_[m] = std::min(noteHz_ * applySpectralTransform(base), nyquistGuard);
            }
            f1_ = freq_[0] > 0.0f ? freq_[0] : noteHz_;
        }

        const float lnK = std::log(1000.0f);
        for (int m = 0; m < numModes_; ++m) {
            // bow target (computed at control rate so it tracks the live Mallet
            // slider + global retune): spatial coupling, a gentle hum tilt, and
            // the same mallet rolloff the impulse uses -> Mallet = shared
            // exciter brightness for impulse AND bow.
            const float mallet = 1.0f / (1.0f + std::pow(freq_[m] / bowCutoff_, 4.0f));
            const float tilt = std::pow(f1_ / freq_[m], 1.3f);  // darker = less harsh
            // level kept below the impulse: the bow SUSTAINS, so polyphonic
            // bowed chords accumulate — needs headroom to not clip
            bowTarget_[m] = 0.5f * bow_ * std::abs(bowWeight_[m]) * mallet * tilt;

            float t60 = releasing_
                ? std::min(damping_.releaseT60, damping_.t60)
                : damping_.t60 * std::pow(f1_ / freq_[m], damping_.tilt);
            if (damping_.comb > 0.0f) {
                // comb notch over mode index: period set by combFreq (wide
                // notches at low freq, every-other-mode at high). Depth is
                // near-total at the top (x0.002) — far more extreme than before.
                const float period = 2.0f + 14.0f * (1.0f - damping_.combFreq);
                const float kill = 0.5f - 0.5f * std::cos(2.0f * (float) M_PI * m / period);
                t60 *= 1.0f - 0.998f * damping_.comb * kill;
            }
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
    static constexpr float kBowLp = 0.20f;  // one-pole noise cutoff (~1.6 kHz @48k)

    float f1_ = 1.0f;
    float bow_ = 0.0f;
    float bowCutoff_ = 2200.0f;
    float impulse_ = 1.0f;
    float impulseAmp_ = 1.0f;
    float bend_ = 1.0f;
    float warp_ = 1.0f;
    float harmonic_ = 0.0f;
    float bowNoise_ = 0.0f;
    int bowAge_ = 0;
    int injectPos_ = 0;
    int injectLen_ = 0;
    uint32_t rng_ = 1;
    const SpectrumFrame* globalFrame_ = nullptr;
    DampingParams damping_;

    float freq_[kMaxModes] = {};
    float ratio_[kMaxModes] = {};  // base (unwarped) f_k/f_1 captured at note-on
    float exc_[kMaxModes] = {};
    float bowWeight_[kMaxModes] = {};   // spatial coupling (fixed at note-on)
    float bowTarget_[kMaxModes] = {};   // live target amplitude (mallet+tilt)
    float s_[kMaxModes] = {};
    float c_[kMaxModes] = {};
    float rc_[kMaxModes] = {};
    float rs_[kMaxModes] = {};
    float rcStep_[kMaxModes] = {};
    float rsStep_[kMaxModes] = {};
};

} // namespace curv
