#include "PluginProcessor.h"

#include "BinaryData.h"
#include "PluginEditor.h"

namespace curv {

namespace {
constexpr int kGeometryPollMs = 50;
} // namespace

CurvSynthProcessor::CurvSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("curv-geometry"),
      apvts(*this, nullptr, "params", createLayout())
{
    pManifold_ = apvts.getRawParameterValue("manifold");
    pStrike_ = apvts.getRawParameterValue("strike");
    pModes_ = apvts.getRawParameterValue("modes");
    pMallet_ = apvts.getRawParameterValue("mallet");
    pT60_ = apvts.getRawParameterValue("t60");
    pTilt_ = apvts.getRawParameterValue("tilt");
    pRelease_ = apvts.getRawParameterValue("release");
    pGain_ = apvts.getRawParameterValue("gain");
    pFlowMode_ = apvts.getRawParameterValue("flowmode");
    pFlowRate_ = apvts.getRawParameterValue("flowrate");
    pKick_ = apvts.getRawParameterValue("kick");
    pVoiceMode_ = apvts.getRawParameterValue("voicemode");
    pBow_ = apvts.getRawParameterValue("bow");
    pReset_ = apvts.getRawParameterValue("reset");
    pPress_ = apvts.getRawParameterValue("press");
    pPressSize_ = apvts.getRawParameterValue("presssize");
    pComb_ = apvts.getRawParameterValue("comb");
    pImpulse_ = apvts.getRawParameterValue("impulse");

    startThread();
}

CurvSynthProcessor::~CurvSynthProcessor()
{
    stopThread(4000);
}

juce::AudioProcessorValueTreeState::ParameterLayout CurvSynthProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray manifolds;
    for (int i = 0; i < (int) PresetId::Count; ++i)
        manifolds.add(presetName((PresetId) i));
    layout.add(std::make_unique<juce::AudioParameterChoice>("manifold", "Manifold", manifolds, 0));

    layout.add(std::make_unique<P>("strike", "Strike Point",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.42f));
    layout.add(std::make_unique<juce::AudioParameterInt>("modes", "Mode Count", 8, kMaxModes, 96));
    layout.add(std::make_unique<P>("mallet", "Mallet Cutoff",
                                   juce::NormalisableRange<float>(200.0f, 12000.0f, 0.0f, 0.35f), 2200.0f));
    layout.add(std::make_unique<P>("t60", "T60",
                                   juce::NormalisableRange<float>(0.1f, 20.0f, 0.0f, 0.4f), 5.0f));
    // negative tilt = anti-acoustic decay (highs outlast lows) — intentional
    layout.add(std::make_unique<P>("tilt", "Damping Tilt",
                                   juce::NormalisableRange<float>(-1.0f, 2.0f), 0.7f));
    layout.add(std::make_unique<P>("release", "Release T60",
                                   juce::NormalisableRange<float>(0.05f, 8.0f, 0.0f, 0.5f), 0.3f));
    layout.add(std::make_unique<P>("gain", "Gain",
                                   juce::NormalisableRange<float>(-36.0f, 6.0f), -6.0f));

    // ---- Phase 2: flow as performance gesture ----
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "flowmode", "Flow", juce::StringArray { "Off", "Relax", "Sharpen" }, 0));
    layout.add(std::make_unique<P>("flowrate", "Flow Rate",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    layout.add(std::make_unique<juce::AudioParameterBool>("kick", "Kick (perturb metric)", false));
    // permanence is a feature (press leaves a trace via clamp saturation);
    // reset is the optional way back to the pristine object
    layout.add(std::make_unique<juce::AudioParameterBool>("reset", "Reset Shape", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "voicemode", "Voices", juce::StringArray { "Snapshot", "Global Flow" }, 1));
    layout.add(std::make_unique<P>("bow", "Bow",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // Phase 3: press = localized curvature injection at the strike point
    // (a bump grows under your finger; Relax heals it)
    layout.add(std::make_unique<P>("press", "Press",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // press bump falloff radius: pointy dent .. broad swell
    layout.add(std::make_unique<P>("presssize", "Press Size",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    // comb = selective absorption: every other mode's decay collapses
    layout.add(std::make_unique<P>("comb", "Damp Comb",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // mallet impulse level, independent of bow (0 + bow = purely bowed)
    layout.add(std::make_unique<P>("impulse", "Impulse",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    return layout;
}

// ------------------------------------------------------------ geometry thread

void CurvSynthProcessor::run()
{
    int lastManifold = -1, lastModes = -1;
    float lastStrike = -1.0f, lastKick = -1.0f, lastReset = -1.0f;
    unsigned kickSeed = 1;
    double flowSinceResolve = 0.0;
    constexpr double kResolveFlowTime = 0.6;  // re-solve cadence in flow time:
                                              // bounds fast-path drift regardless of rate
    constexpr double kMaxDt = 0.25;           // per-step flow time at rate 1.0

    while (!threadShouldExit()) {
        const int manifold = (int) pManifold_->load();
        const int modes = (int) pModes_->load();
        const float strike = pStrike_->load();
        const int flowMode = (int) pFlowMode_->load();   // 0 off, 1 relax, 2 sharpen
        const float flowRate = pFlowRate_->load();
        const float kick = pKick_->load();

        bool publish = false;

        if (manifold != lastManifold) {
            geometry_.loadPreset((PresetId) manifold,
                                 BinaryData::genus2_obj, (size_t) BinaryData::genus2_objSize);
            flowSinceResolve = 0.0;
            publish = true;
        }
        if (modes != lastModes || strike != lastStrike)
            publish = true;

        // every toggle of the Kick parameter = one conformal perturbation
        if (lastKick >= 0.0f && kick != lastKick) {
            geometry_.flowKick(0.6, kickSeed++);
            geometry_.resolve();  // fresh basis on the kicked metric
            flowSinceResolve = 0.0;
            publish = true;
        }
        lastKick = kick;

        // every toggle of Reset Shape = restore the pristine base metric
        const float reset = pReset_->load();
        if (lastReset >= 0.0f && reset != lastReset) {
            geometry_.flowReset();
            flowSinceResolve = 0.0;
            publish = true;
        }
        lastReset = reset;

        const float press = pPress_->load();
        if (press > 0.001f) {
            const double sigma = 0.8 + 5.2 * (double) pPressSize_->load();
            geometry_.flowPress(strike, 2.5 * press, 0.05, sigma);
            flowSinceResolve += 0.05;
            publish = true;
        }

        if (flowMode != 0 && flowRate > 0.0f) {
            // reverse flow amplifies curvature deviation — but uniform
            // curvature is an exact fixed point, so SHARPEN from the base
            // metric would sit still forever. Seed a tiny symmetry-breaking
            // perturbation when engaging near equilibrium (reverse
            // diffusion amplifying noise, which is what it physically is).
            if (flowMode == 2 && geometry_.curvatureError() < 0.01)
                geometry_.flowKick(0.05, kickSeed++);
            const double dt = kMaxDt * flowRate * flowRate;
            flowSinceResolve += geometry_.flowStep(dt, flowMode == 1 ? +1.0 : -1.0);
            publish = true;
        }
        if (flowSinceResolve >= kResolveFlowTime) {
            geometry_.resolve();
            flowSinceResolve = 0.0;
        }

        if (publish) {
            geometry_.fillFrame(bus_.beginWrite(), modes, strike);
            bus_.publish();
            geometry_.fillVizFrame(vizBus_.beginWrite(), modes, strike, manifold);
            vizBus_.publish();
            lastManifold = manifold;
            lastModes = modes;
            lastStrike = strike;
        }
        wait(kGeometryPollMs >> ((flowMode != 0 || press > 0.001f) ? 1 : 0));
    }
}

// ---------------------------------------------------------------- audio thread

void CurvSynthProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    voices_.prepare(sampleRate);
    gainSmoothed_.reset(sampleRate, 0.02);
    gainSmoothed_.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(pGain_->load()));
}

void CurvSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // pull the newest spectrum frame; copy because the bus slot is recycled
    const SpectrumFrame* latest = nullptr;
    if (bus_.readLatest(&latest))
        currentFrame_ = *latest;
    const bool haveSpectrum = currentFrame_.numModes > 0 || latest != nullptr;

    voices_.setDamping({ pT60_->load(), pTilt_->load(), pRelease_->load(), pComb_->load() });
    voices_.setGlobalFrame(((int) pVoiceMode_->load() == 1 && currentFrame_.numModes > 0)
                               ? &currentFrame_ : nullptr);
    voices_.setBow(pBow_->load());
    voices_.setImpulse(pImpulse_->load());
    const float mallet = pMallet_->load();
    gainSmoothed_.setTargetValue(juce::Decibels::decibelsToGain(pGain_->load()));

    float* out = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();
    int pos = 0;

    for (const auto metadata : midi) {
        const auto msg = metadata.getMessage();
        const int at = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (at > pos) {
            voices_.renderAdd(out + pos, at - pos);
            pos = at;
        }
        if (msg.isNoteOn() && haveSpectrum && currentFrame_.numModes > 0)
            voices_.noteOn(currentFrame_, msg.getNoteNumber(), msg.getFloatVelocity(), mallet);
        else if (msg.isNoteOff())
            voices_.noteOff(msg.getNoteNumber());
        else if (msg.isPitchWheel())
            voices_.setPitchBend(std::pow(2.0f,
                (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f / 12.0f));
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            voices_.allNotesOff();
    }
    if (pos < numSamples)
        voices_.renderAdd(out + pos, numSamples - pos);

    voices_.updateActivity();

    // gain + soft-clip safety, then fan out to all channels
    for (int i = 0; i < numSamples; ++i) {
        const float g = gainSmoothed_.getNextValue();
        out[i] = std::tanh(out[i] * g);
    }
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, out, numSamples);
}

// ------------------------------------------------------------------- state

void CurvSynthProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CurvSynthProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* CurvSynthProcessor::createEditor()
{
    return new CurvSynthEditor(*this);
}

} // namespace curv

// JUCE entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new curv::CurvSynthProcessor();
}
