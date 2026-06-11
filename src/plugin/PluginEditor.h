// Phase 3/4 editor: the manifold is the interface. Left, the mesh rendered
// with a curvature heatmap (coral = curvature above target, teal = below)
// and conformal bulge, drag-to-rotate, click-to-set-strike-point. Right, the
// live spectrum as scrolling partial traces plus the parameter panel.
// All geometry state arrives via the VizBus (message thread is the sole
// consumer); rendering is software juce::Graphics at 30 Hz.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../engine/VizFrame.h"
#include "../geometry/Presets.h"
#include "PluginProcessor.h"

namespace curv {

class ManifoldView : public juce::Component, private juce::Timer
{
public:
    explicit ManifoldView(CurvSynthProcessor& proc);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    int numVerts() const { return frame_.numVerts; }
    float curvatureErr() const { return frame_.curvatureErr; }

private:
    void timerCallback() override;
    void rebuildMeshIfNeeded(int presetId);
    juce::Point<float> project(int vi, float& depth) const;

    CurvSynthProcessor& proc_;
    VizFrame frame_;
    Mesh displayMesh_;
    int meshPresetId_ = -1;
    float yaw_ = 0.6f, pitch_ = 0.25f;
    float meshExtent_ = 1.0f;
    int extentPresetId_ = -1;
    juce::Point<float> lastDrag_;
    bool dragMoved_ = false;
    std::vector<int> faceOrder_;
};

class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView(CurvSynthProcessor& proc);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    CurvSynthProcessor& proc_;
    static constexpr int kHist = 160;
    float hist_[kHist][kMaxModes] = {};
    int histLen_ = 0;
    int numModes_ = 0;
    float curvErr_ = 0.0f;
};

class CurvSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit CurvSynthEditor(CurvSynthProcessor& proc);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void addSlider(const juce::String& paramId, const juce::String& label,
                   const juce::String& suffix);
    juce::String buildStateReport() const;

    CurvSynthProcessor& proc_;
    ManifoldView manifold_;
    SpectrumView spectrum_;

    juce::ComboBox manifoldBox_, flowBox_, voiceBox_;
    juce::ToggleButton kickButton_ { "Kick" };
    juce::TextButton copyButton_ { "Copy state" };
    std::vector<std::unique_ptr<juce::Slider>> sliders_;
    std::vector<std::unique_ptr<juce::Label>> labels_;
    std::vector<std::unique_ptr<SA>> sliderAtts_;
    std::unique_ptr<CA> manifoldAtt_, flowAtt_, voiceAtt_;
    std::unique_ptr<BA> kickAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurvSynthEditor)
};

} // namespace curv
