#include "PluginEditor.h"

#include "BinaryData.h"

namespace curv {

namespace {

juce::Colour curvatureColour(float dev, float shade)
{
    const float t = juce::jlimit(-1.0f, 1.0f, dev / 0.45f);
    const juce::Colour neutral(0xFF8a8880), coral(0xFFD85A30), teal(0xFF1D9E75);
    const juce::Colour c = t >= 0 ? neutral.interpolatedWith(coral, t)
                                  : neutral.interpolatedWith(teal, -t);
    return c.withMultipliedBrightness(shade);
}

} // namespace

// ------------------------------------------------------------ ManifoldView

ManifoldView::ManifoldView(CurvSynthProcessor& proc) : proc_(proc)
{
    startTimerHz(30);
}

void ManifoldView::timerCallback()
{
    const VizFrame* latest = nullptr;
    if (proc_.vizBus().readLatest(&latest) && latest != nullptr)
        frame_ = *latest;
    if (frame_.presetId >= 0)
        rebuildMeshIfNeeded(frame_.presetId);
    repaint();
}

void ManifoldView::rebuildMeshIfNeeded(int presetId)
{
    if (presetId == meshPresetId_)
        return;
    displayMesh_ = makePreset((PresetId) presetId,
                              BinaryData::genus2_obj, (size_t) BinaryData::genus2_objSize);
    meshPresetId_ = presetId;
    faceOrder_.resize(displayMesh_.F.size());
}

juce::Point<float> ManifoldView::project(int vi, float& depth) const
{
    const auto& v = displayMesh_.V[(size_t) vi];
    const float bulge = vi < frame_.numVerts ? 1.0f + 0.5f * frame_.uDev[vi] : 1.0f;
    const float x = (float) v[0] * bulge, y = (float) v[1] * bulge, z = (float) v[2] * bulge;
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float cp = std::cos(pitch_), sp = std::sin(pitch_);
    const float x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
    const float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    const float scale = 0.36f * std::min(getWidth(), getHeight());
    // display embeddings are roughly unit-ish; tori/genus2 are wider, so
    // normalize by a per-preset fudge from the mesh extent
    depth = z2;
    return { getWidth() * 0.5f + x1 * scale / meshExtent_,
             getHeight() * 0.5f - y2 * scale / meshExtent_ };
}

void ManifoldView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF161512));
    if (meshPresetId_ < 0 || displayMesh_.numFaces() == 0)
        return;

    // recompute extent once per mesh (cheap; n small)
    if (extentPresetId_ != meshPresetId_) {
        float ext = 0.0f;
        for (const auto& v : displayMesh_.V)
            ext = std::max(ext, (float) std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]));
        meshExtent_ = std::max(ext, 0.1f);
        extentPresetId_ = meshPresetId_;
    }

    const int nv = displayMesh_.numVertices();
    std::vector<juce::Point<float>> pts((size_t) nv);
    std::vector<float> depth((size_t) nv);
    for (int i = 0; i < nv; ++i)
        pts[(size_t) i] = project(i, depth[(size_t) i]);

    const auto& F = displayMesh_.F;
    for (size_t fi = 0; fi < F.size(); ++fi)
        faceOrder_[fi] = (int) fi;
    std::sort(faceOrder_.begin(), faceOrder_.end(), [&](int a, int b) {
        const float za = depth[(size_t) F[(size_t) a][0]] + depth[(size_t) F[(size_t) a][1]] + depth[(size_t) F[(size_t) a][2]];
        const float zb = depth[(size_t) F[(size_t) b][0]] + depth[(size_t) F[(size_t) b][1]] + depth[(size_t) F[(size_t) b][2]];
        return za < zb;
    });

    for (int fi : faceOrder_) {
        const auto& f = F[(size_t) fi];
        const auto p0 = pts[(size_t) f[0]], p1 = pts[(size_t) f[1]], p2 = pts[(size_t) f[2]];
        const float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
        if (cross >= 0.0f)
            continue;  // back-face (screen y is flipped, so front faces wind negative)
        float dev = 0.0f;
        for (int j = 0; j < 3; ++j)
            if (f[j] < frame_.numVerts)
                dev += frame_.kDev[f[j]] / 3.0f;
        const float shade = 0.6f + 0.4f * std::min(1.0f, -cross * 1800.0f / (float) (getWidth() * getHeight()));
        juce::Path tri;
        tri.addTriangle(p0, p1, p2);
        g.setColour(curvatureColour(dev, shade));
        g.fillPath(tri);
    }

    if (frame_.strikeVertex < nv && depth[(size_t) frame_.strikeVertex] > -0.05f) {
        const auto sp = pts[(size_t) frame_.strikeVertex];
        g.setColour(juce::Colour(0xFF7F77DD));
        g.fillEllipse(sp.x - 6, sp.y - 6, 12, 12);
        g.setColour(juce::Colour(0xFFEEEDFE));
        g.drawEllipse(sp.x - 6, sp.y - 6, 12, 12, 1.5f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(12.0f);
    g.drawText("drag to rotate - click to strike here - coral bumps / teal dents",
               getLocalBounds().removeFromBottom(18), juce::Justification::centred);
}

void ManifoldView::mouseDown(const juce::MouseEvent& e)
{
    lastDrag_ = e.position;
    dragMoved_ = false;
}

void ManifoldView::mouseDrag(const juce::MouseEvent& e)
{
    const auto d = e.position - lastDrag_;
    if (std::abs(d.x) + std::abs(d.y) > 3.0f)
        dragMoved_ = true;
    yaw_ += d.x * 0.01f;
    pitch_ = juce::jlimit(-1.4f, 1.4f, pitch_ + d.y * 0.01f);
    lastDrag_ = e.position;
    repaint();
}

void ManifoldView::mouseUp(const juce::MouseEvent& e)
{
    if (dragMoved_ || meshPresetId_ < 0)
        return;
    const int nv = displayMesh_.numVertices();
    int best = -1;
    float bestDist = 30.0f * 30.0f;
    for (int i = 0; i < nv; ++i) {
        float depth = 0.0f;
        const auto p = project(i, depth);
        if (depth < 0.0f)
            continue;
        const float d2 = (p.x - e.position.x) * (p.x - e.position.x)
                         + (p.y - e.position.y) * (p.y - e.position.y);
        if (d2 < bestDist) {
            bestDist = d2;
            best = i;
        }
    }
    if (best >= 0)
        if (auto* p = proc_.apvts.getParameter("strike"))
            p->setValueNotifyingHost((float) best / (float) std::max(nv - 1, 1));
}

// ------------------------------------------------------------ SpectrumView

SpectrumView::SpectrumView(CurvSynthProcessor& proc) : proc_(proc)
{
    startTimerHz(30);
}

void SpectrumView::timerCallback()
{
    const VizFrame* latest = nullptr;
    proc_.vizBus().readLatest(&latest);
    if (latest != nullptr) {
        numModes_ = latest->numModes;
        curvErr_ = latest->curvatureErr;
        if (histLen_ == kHist)
            std::memmove(hist_[0], hist_[1], sizeof(float) * kMaxModes * (kHist - 1));
        else
            ++histLen_;
        std::memcpy(hist_[histLen_ - 1], latest->ratio, sizeof(float) * kMaxModes);
    }
    repaint();
}

void SpectrumView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF161512));
    if (histLen_ < 2)
        return;
    const float w = (float) getWidth(), h = (float) getHeight();
    auto fy = [h](float ratio) {
        return h - 14.0f - (std::log(juce::jlimit(0.8f, 40.0f, ratio) / 0.8f)
                            / std::log(40.0f / 0.8f)) * (h - 28.0f);
    };

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.setFont(11.0f);
    for (float r : { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f }) {
        const float y = fy(r);
        g.drawHorizontalLine((int) y, 26.0f, w);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawText(juce::String(r, 0) + "x", 0, (int) y - 7, 24, 14, juce::Justification::right);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
    }

    juce::Path path;
    for (int m = 0; m < numModes_; ++m) {
        path.clear();
        for (int i = 0; i < histLen_; ++i) {
            const float x = 26.0f + (w - 26.0f) * (float) i / (float) (kHist - 1);
            const float y = fy(hist_[i][m]);
            i == 0 ? path.startNewSubPath(x, y) : path.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xFF7F77DD).withAlpha(m == 0 ? 0.95f : 0.45f));
        g.strokePath(path, juce::PathStrokeType(m == 0 ? 2.0f : 1.2f));
    }

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);
    g.drawText("partials f_k / f_1    max|K-Kbar| = " + juce::String(curvErr_, 3),
               getLocalBounds().removeFromTop(16), juce::Justification::centredRight);
}

// ------------------------------------------------------------ editor shell

CurvSynthEditor::CurvSynthEditor(CurvSynthProcessor& proc)
    : AudioProcessorEditor(proc), proc_(proc), manifold_(proc), spectrum_(proc)
{
    addAndMakeVisible(manifold_);
    addAndMakeVisible(spectrum_);

    auto setupBox = [this](juce::ComboBox& box, const char* paramId, std::unique_ptr<CA>& att) {
        addAndMakeVisible(box);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(proc_.apvts.getParameter(paramId)))
            box.addItemList(p->choices, 1);
        att = std::make_unique<CA>(proc_.apvts, paramId, box);
    };
    setupBox(manifoldBox_, "manifold", manifoldAtt_);
    setupBox(flowBox_, "flowmode", flowAtt_);
    setupBox(voiceBox_, "voicemode", voiceAtt_);

    addAndMakeVisible(kickButton_);
    kickAtt_ = std::make_unique<BA>(proc_.apvts, "kick", kickButton_);

    for (auto [id, name] : { std::pair { "strike", "Strike" }, { "modes", "Modes" },
                             { "mallet", "Mallet" }, { "t60", "T60" }, { "tilt", "Tilt" },
                             { "release", "Release" }, { "flowrate", "Flow Rate" },
                             { "press", "Press" }, { "bow", "Bow" }, { "comb", "Comb" },
                             { "gain", "Gain" } })
        addSlider(id, name);

    setSize(960, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1600, 1000);
}

void CurvSynthEditor::addSlider(const juce::String& paramId, const juce::String& label)
{
    auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                 juce::Slider::TextBoxRight);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    addAndMakeVisible(*slider);
    sliderAtts_.push_back(std::make_unique<SA>(proc_.apvts, paramId, *slider));

    auto lab = std::make_unique<juce::Label>(juce::String(), label);
    lab->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*lab);

    sliders_.push_back(std::move(slider));
    labels_.push_back(std::move(lab));
}

void CurvSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF201F1B));
}

void CurvSynthEditor::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto left = area.removeFromLeft(juce::roundToInt(area.getWidth() * 0.46f));
    manifold_.setBounds(left);
    area.removeFromLeft(8);

    auto right = area;
    spectrum_.setBounds(right.removeFromTop(juce::roundToInt(right.getHeight() * 0.42f)));
    right.removeFromTop(8);

    auto row = right.removeFromTop(26);
    const int boxW = row.getWidth() / 4;
    manifoldBox_.setBounds(row.removeFromLeft(boxW + 30).reduced(2));
    flowBox_.setBounds(row.removeFromLeft(boxW - 20).reduced(2));
    voiceBox_.setBounds(row.removeFromLeft(boxW).reduced(2));
    kickButton_.setBounds(row.reduced(2));

    const int rowH = juce::jmax(20, right.getHeight() / juce::jmax(1, (int) sliders_.size()));
    for (size_t i = 0; i < sliders_.size(); ++i) {
        auto r = right.removeFromTop(rowH);
        labels_[i]->setBounds(r.removeFromLeft(70));
        sliders_[i]->setBounds(r);
    }
}

} // namespace curv
