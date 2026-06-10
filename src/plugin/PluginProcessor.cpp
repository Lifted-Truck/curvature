#include "PluginProcessor.h"

#include "BinaryData.h"

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
    return layout;
}

// ------------------------------------------------------------ geometry thread

void CurvSynthProcessor::run()
{
    int lastManifold = -1, lastModes = -1;
    float lastStrike = -1.0f;

    while (!threadShouldExit()) {
        const int manifold = (int) pManifold_->load();
        const int modes = (int) pModes_->load();
        const float strike = pStrike_->load();

        if (manifold != lastManifold || modes != lastModes || strike != lastStrike) {
            if (manifold != lastManifold)
                geometry_.loadPreset((PresetId) manifold,
                                     BinaryData::genus2_obj, (size_t) BinaryData::genus2_objSize);
            geometry_.fillFrame(bus_.beginWrite(), modes, strike);
            bus_.publish();
            lastManifold = manifold;
            lastModes = modes;
            lastStrike = strike;
        }
        wait(kGeometryPollMs);
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

    voices_.setDamping({ pT60_->load(), pTilt_->load(), pRelease_->load() });
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
    return new juce::GenericAudioProcessorEditor(*this);
}

} // namespace curv

// JUCE entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new curv::CurvSynthProcessor();
}
