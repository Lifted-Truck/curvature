#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../engine/GeometryService.h"
#include "../engine/SpectrumBus.h"
#include "../engine/StrikeQueue.h"
#include "../engine/VizFrame.h"
#include "../engine/VoiceManager.h"

namespace curv {

class CurvSynthProcessor : public juce::AudioProcessor,
                           private juce::Thread
{
public:
    CurvSynthProcessor();
    ~CurvSynthProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "curvsynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    VizBus& vizBus() { return vizBus_; }
    float outputPeak() const { return outputPeak_.load(std::memory_order_relaxed); }

private:
    void run() override;  // geometry thread

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    GeometryService geometry_;        // geometry thread only (after construction)
    SpectrumBus bus_;
    VizBus vizBus_;
    StrikeQueue<64> strikes_;         // audio thread -> geometry thread
    VoiceManager voices_;
    SpectrumFrame currentFrame_;      // audio thread copy

    std::atomic<float>* pManifold_ = nullptr;
    std::atomic<float>* pStrike_ = nullptr;
    std::atomic<float>* pModes_ = nullptr;
    std::atomic<float>* pMallet_ = nullptr;
    std::atomic<float>* pT60_ = nullptr;
    std::atomic<float>* pTilt_ = nullptr;
    std::atomic<float>* pRelease_ = nullptr;
    std::atomic<float>* pGain_ = nullptr;
    std::atomic<float>* pFlowMode_ = nullptr;   // Off / Relax / Sharpen / Manual
    std::atomic<float>* pFlowRate_ = nullptr;
    std::atomic<float>* pSharpness_ = nullptr;  // Manual mode servo target
    std::atomic<float>* pKick_ = nullptr;       // edge-triggered conformal kick
    std::atomic<float>* pVoiceMode_ = nullptr;  // Snapshot / Global
    std::atomic<float>* pBow_ = nullptr;
    std::atomic<float>* pReset_ = nullptr;      // edge-triggered: restore base metric
    std::atomic<float>* pPress_ = nullptr;      // localized curvature injection
    std::atomic<float>* pPressSize_ = nullptr;  // bump falloff radius
    std::atomic<float>* pComb_ = nullptr;
    std::atomic<float>* pCombFreq_ = nullptr;   // notch spacing over mode index
    std::atomic<float>* pImpulse_ = nullptr;    // mallet level, independent of bow
    std::atomic<float>* pMemory_ = nullptr;     // 1 = full patina, 0 = elastic
    std::atomic<float>* pMemRate_ = nullptr;    // patina-decay speed (own clock)
    std::atomic<float>* pWarp_ = nullptr;       // spectral dispersion exponent
    std::atomic<float>* pStrikeDeform_ = nullptr;  // how much each strike dents
    std::atomic<float>* pStrikeRipple_ = nullptr;  // propagating wave per strike
    std::atomic<float>* pRippleSpeed_ = nullptr;   // wave propagation speed

    std::atomic<float> outputPeak_ { 0.0f };    // pre-saturator peak, for the meter

    juce::SmoothedValue<float> gainSmoothed_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurvSynthProcessor)
};

} // namespace curv
