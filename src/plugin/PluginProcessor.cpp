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
    pSharpness_ = apvts.getRawParameterValue("sharpness");
    pKick_ = apvts.getRawParameterValue("kick");
    pVoiceMode_ = apvts.getRawParameterValue("voicemode");
    pBow_ = apvts.getRawParameterValue("bow");
    pReset_ = apvts.getRawParameterValue("reset");
    pPress_ = apvts.getRawParameterValue("press");
    pPressSize_ = apvts.getRawParameterValue("presssize");
    pComb_ = apvts.getRawParameterValue("comb");
    pImpulse_ = apvts.getRawParameterValue("impulse");
    pMemory_ = apvts.getRawParameterValue("memory");
    pMemRate_ = apvts.getRawParameterValue("memrate");
    pWarp_ = apvts.getRawParameterValue("warp");
    pStrikeDeform_ = apvts.getRawParameterValue("strikedeform");
    pStrikeRipple_ = apvts.getRawParameterValue("strikeripple");

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
                                   juce::NormalisableRange<float>(-1.0f, 2.0f), 0.85f));
    layout.add(std::make_unique<P>("release", "Release T60",
                                   juce::NormalisableRange<float>(0.05f, 8.0f, 0.0f, 0.5f), 0.3f));
    layout.add(std::make_unique<P>("gain", "Gain",
                                   juce::NormalisableRange<float>(-36.0f, 6.0f), -9.0f));

    // ---- Phase 2: flow as performance gesture ----
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "flowmode", "Flow", juce::StringArray { "Off", "Relax", "Sharpen", "Manual" }, 0));
    // Flow Rate is the global geometry clock: flow steps, the Manual servo,
    // and Memory's elastic restoring force all scale with it
    layout.add(std::make_unique<P>("flowrate", "Flow Rate",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    // Manual mode: position control — the slider sets a target curvature
    // concentration and the flow servos toward it (smooth <-> sharp)
    layout.add(std::make_unique<P>("sharpness", "Sharpness",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("kick", "Kick (perturb metric)", false));
    // permanence is a feature (press leaves a trace via clamp saturation);
    // reset is the optional way back to the pristine object
    layout.add(std::make_unique<juce::AudioParameterBool>("reset", "Reset Shape", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "voicemode", "Voices", juce::StringArray { "Snapshot", "Global Flow" }, 1));
    layout.add(std::make_unique<P>("bow", "Bow",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // Phase 3: press = localized metric deformation at the strike point.
    // Bipolar — positive concentrates curvature (sharp/bright), negative
    // diffuses it (smooth/round). 0 = no press.
    layout.add(std::make_unique<P>("press", "Press (smooth<>sharp)",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    // press bump falloff radius: pointy dent .. broad swell
    layout.add(std::make_unique<P>("presssize", "Press Size",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    // comb = selective absorption: every other mode's decay collapses
    layout.add(std::make_unique<P>("comb", "Damp Comb",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // mallet impulse level, independent of bow (0 + bow = purely bowed)
    layout.add(std::make_unique<P>("impulse", "Impulse",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    // memory: 1 = deformations are permanent (the object scars), 0 = elastic
    layout.add(std::make_unique<P>("memory", "Memory",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    // memory rate: how fast the patina decays (own clock, not Flow Rate)
    layout.add(std::make_unique<P>("memrate", "Memory Rate",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    // spectral warp: f_k = f1*(f_k/f1)^warp — 1 = physical sqrt-lambda law,
    // >1 stretches into impossible spectra, <1 compresses toward a unison hum
    layout.add(std::make_unique<P>("warp", "Spectral Warp",
                                   juce::NormalisableRange<float>(0.2f, 2.5f), 1.0f));
    // strike-responsive deformation: each note-on dents the manifold inward
    // at the strike point, velocity-scaled — the performance reshapes the
    // instrument (healed per Memory; at Memory = 1 the object scars)
    layout.add(std::make_unique<P>("strikedeform", "Strike Deform",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // strike ripple: each note-on sends a transient wave across the surface
    // that propagates and damps out (a shimmer after the hit)
    layout.add(std::make_unique<P>("strikeripple", "Strike Ripple",
                                   juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return layout;
}

// ------------------------------------------------------------ geometry thread

void CurvSynthProcessor::run()
{
    int lastManifold = -1, lastModes = -1;
    float lastStrike = -1.0f, lastKick = -1.0f, lastReset = -1.0f;
    unsigned kickSeed = 1;
    double flowSinceResolve = 0.0;
    double manualTarget = 0.0;
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

        // crash armor: an eigensolve failure on an extreme metric must never
        // take down the host. Recover by forcing a preset reload (fresh base
        // metric) on the next iteration — the object snaps back, audio and
        // MIDI keep running.
        try {

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
        if (std::abs(press) > 0.001f) {
            const double sigma = 0.8 + 5.2 * (double) pPressSize_->load();
            geometry_.flowPress(strike, 2.5 * press, 0.05, sigma);  // sign: + sharp, - smooth
            flowSinceResolve += 0.05;
            publish = true;
        }

        // strike-responsive deformation: drain note-on hits. Each dents the
        // manifold INWARD (negative press) at the strike point and/or kicks a
        // propagating ripple, velocity-scaled. Dent heals per Memory; ripple
        // damps out on its own.
        const float strikeDeform = pStrikeDeform_->load();
        const float strikeRipple = pStrikeRipple_->load();
        StrikeEvent hit;
        for (int drained = 0; drained < 16 && strikes_.pop(hit); ++drained) {
            if (strikeDeform > 0.001f)  // gentle, varied, self-correcting localized kick
                geometry_.strikeKick(hit.strikeParam,
                                     0.45 * strikeDeform * hit.velocity, kickSeed++);
            if (strikeRipple > 0.001f)  // strong injection — let it be pushed to the limit
                geometry_.rippleStrike(hit.strikeParam, 1.4 * strikeRipple * hit.velocity);
            flowSinceResolve += 0.05;
            publish = true;
        }
        // advance the ripple wave while it has energy (propagates + damps)
        if (geometry_.rippleActive()) {
            geometry_.rippleStep(0.03, 6.0, 2.0);
            publish = true;
        }

        bool manualMode = (flowMode == 3);
        if (manualMode && flowRate > 0.0f) {
            // Manual = a bidirectional position control that scrubs the object
            // along the Ricci flow. Up runs reverse flow (sharpen, concentrate
            // curvature); down runs FORWARD flow (relax — literally walking
            // back down the flow toward uniform curvature, smoothing). Driven
            // on RMS curvature (smooth — unlike max|K-Kbar|, which hops between
            // vertices and was the jitter source) toward a reachable target
            // (plateau ~0.6-0.78 across presets, so 0.5 is always reachable ->
            // settles, no endless chase at full-up). Gated by Flow Rate.
            const float sharp = pSharpness_->load();
            const double target = 0.5 * sharp * sharp;
            manualTarget += 0.2 * (target - manualTarget);  // light slew
            const double rms = geometry_.curvatureRms();
            const double err = manualTarget - rms;
            const double dt = kMaxDt * flowRate * flowRate * std::min(1.0, std::abs(err) / 0.2);
            if (err > 0.01) {        // too smooth: reverse flow up toward target
                if (rms < 0.01)
                    geometry_.flowKick(0.05, kickSeed++);  // seed at equilibrium
                flowSinceResolve += geometry_.flowStep(dt, -1.0);
                publish = true;
            } else if (err < -0.01) {  // too sharp: forward flow back down
                flowSinceResolve += geometry_.flowStep(dt, +1.0);
                publish = true;
            }
        } else if (flowMode != 0 && flowMode != 3 && flowRate > 0.0f) {
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
        // Memory: elastic restoring force toward the base metric (1 =
        // deformations permanent, < 1 springs back at Memory Rate). In Manual
        // mode it's scaled by (1-sharpness)^2 so it heals presses/strike-dents
        // at neutral sharpness but yields to the servo as you sharpen (full
        // strength would fight a held sharp state — the churn from before).
        const float memory = pMemory_->load();
        const float memRate = pMemRate_->load();
        float memWeight = 1.0f;
        if (manualMode) {
            const float sharp = pSharpness_->load();
            memWeight = (1.0f - sharp) * (1.0f - sharp);
        }
        bool elastic = false;
        if (memory < 0.999f && memRate > 0.0f && memWeight > 0.001f
            && geometry_.metricDeviation() > 1e-4) {
            const double rate = 0.9 * (double) (memRate * memRate)
                                * (1.0 - memory) * (1.0 - memory) * memWeight;
            geometry_.flowElastic(rate);
            flowSinceResolve += rate;
            elastic = true;
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

        const bool busy = flowMode != 0 || std::abs(press) > 0.001f || elastic
                          || strikeDeform > 0.001f || strikeRipple > 0.001f
                          || geometry_.rippleActive();
        wait(kGeometryPollMs >> (busy ? 1 : 0));
        } catch (...) {
            lastManifold = -1;       // force reload + republish next iteration
            flowSinceResolve = 0.0;
            wait(100);
        }
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
    voices_.setWarp(pWarp_->load());
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
        if (msg.isNoteOn() && haveSpectrum && currentFrame_.numModes > 0) {
            voices_.noteOn(currentFrame_, msg.getNoteNumber(), msg.getFloatVelocity(), mallet);
            // strike-responsive deformation: hand the hit to the geometry
            // thread (lock-free; never blocks audio). It dents / ripples the
            // manifold at the strike point on its next pass.
            if (pStrikeDeform_->load() > 0.001f || pStrikeRipple_->load() > 0.001f)
                strikes_.push({ pStrike_->load(), msg.getFloatVelocity() });
        }
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

    // gain + soft-clip safety; track the post-gain pre-saturator peak so the
    // editor meter warns about headroom *before* the tanh starts crunching
    float blockPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        const float g = gainSmoothed_.getNextValue();
        const float pre = out[i] * g;
        blockPeak = std::max(blockPeak, std::abs(pre));
        out[i] = std::tanh(pre);
    }
    const float prev = outputPeak_.load(std::memory_order_relaxed);
    // fast attack, slow release so brief peaks stay visible
    outputPeak_.store(blockPeak > prev ? blockPeak : prev * 0.85f + blockPeak * 0.15f,
                      std::memory_order_relaxed);

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
