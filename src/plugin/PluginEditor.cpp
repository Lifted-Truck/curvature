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

ManifoldView::ManifoldView(CurvSynthProcessor& proc) : proc_(proc) {}

void ManifoldView::setFrame(const VizFrame& f)
{
    frame_ = f;
    if (frame_.presetId >= 0)
        rebuildMeshIfNeeded(frame_.presetId);
    repaint();
}

void ManifoldView::rebuildMeshIfNeeded(int presetId)
{
    if (presetId == meshPresetId_)
        return;
    meshPresetId_ = presetId;
    is4DView_ = presetIs4D((PresetId) presetId);
    if (is4DView_) {
        tetMesh_ = makeTetPreset((PresetId) presetId);
        return;  // wireframe path needs no triangle mesh / normals
    }

    const char* obj = BinaryData::genus2_obj;
    size_t objSize = (size_t) BinaryData::genus2_objSize;
    if ((PresetId) presetId == PresetId::Mandelbulb) {
        obj = BinaryData::mandelbulb_obj;
        objSize = (size_t) BinaryData::mandelbulb_objSize;
    }
    displayMesh_ = makePreset((PresetId) presetId, obj, objSize);
    faceOrder_.resize(displayMesh_.F.size());

    // area-weighted vertex normals: press bulges displace along the surface
    // normal, so a press reads as "pushing out of the surface" everywhere
    // (radial displacement pushed inward on the inner wall of a torus)
    const int nv = displayMesh_.numVertices();
    vertexNormals_.assign((size_t) nv, { 0.0f, 0.0f, 0.0f });
    for (const auto& f : displayMesh_.F) {
        const auto &a = displayMesh_.V[(size_t) f[0]], &b = displayMesh_.V[(size_t) f[1]],
                   &c = displayMesh_.V[(size_t) f[2]];
        const double e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        const double e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        const double nx = e1[1] * e2[2] - e1[2] * e2[1];
        const double ny = e1[2] * e2[0] - e1[0] * e2[2];
        const double nz = e1[0] * e2[1] - e1[1] * e2[0];
        for (int j = 0; j < 3; ++j) {
            vertexNormals_[(size_t) f[j]][0] += (float) nx;
            vertexNormals_[(size_t) f[j]][1] += (float) ny;
            vertexNormals_[(size_t) f[j]][2] += (float) nz;
        }
    }
    for (auto& nrm : vertexNormals_) {
        const float len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
        if (len > 1e-12f)
            for (auto& x : nrm)
                x /= len;
    }
}

juce::Point<float> ManifoldView::project(int vi, float& depth) const
{
    float x, y, z;
    if (is4DView_) {
        // 3-torus lattice: center the cube, displace each vertex radially by
        // its conformal factor so the lattice "breathes" (no surface normal)
        const auto& v = tetMesh_.V[(size_t) vi];
        const float px = (float) v[0] - 0.5f * (float) tetMesh_.a;
        const float py = (float) v[1] - 0.5f * (float) tetMesh_.b;
        const float pz = (float) v[2] - 0.5f * (float) tetMesh_.c;
        const float rlen = std::sqrt(px * px + py * py + pz * pz) + 1e-6f;
        const float d = (vi < frame_.numVerts ? 0.5f * frame_.uDev[vi] : 0.0f) * meshExtent_;
        x = px + px / rlen * d;
        y = py + py / rlen * d;
        z = pz + pz / rlen * d;
    } else {
        const auto& v = displayMesh_.V[(size_t) vi];
        const auto& nrm = vertexNormals_[(size_t) vi];
        const float d = (vi < frame_.numVerts ? 0.35f * frame_.uDev[vi] : 0.0f) * meshExtent_;
        x = (float) v[0] + nrm[0] * d;
        y = (float) v[1] + nrm[1] * d;
        z = (float) v[2] + nrm[2] * d;
    }
    const float cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float cp = std::cos(pitch_), sp = std::sin(pitch_);
    const float x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
    const float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    const float scale = 0.36f * zoom_ * std::min(getWidth(), getHeight());
    // display embeddings are roughly unit-ish; tori/genus2 are wider, so
    // normalize by a per-preset fudge from the mesh extent
    depth = z2;
    return { getWidth() * 0.5f + x1 * scale / meshExtent_,
             getHeight() * 0.5f - y2 * scale / meshExtent_ };
}

void ManifoldView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF161512));
    if (meshPresetId_ < 0)
        return;

    // recompute extent once per mesh (cheap; n small)
    if (extentPresetId_ != meshPresetId_) {
        float ext = 0.0f;
        if (is4DView_) {
            const float cx = 0.5f * (float) tetMesh_.a, cy = 0.5f * (float) tetMesh_.b,
                        cz = 0.5f * (float) tetMesh_.c;
            for (const auto& v : tetMesh_.V)
                ext = std::max(ext, std::sqrt(((float) v[0] - cx) * ((float) v[0] - cx)
                                              + ((float) v[1] - cy) * ((float) v[1] - cy)
                                              + ((float) v[2] - cz) * ((float) v[2] - cz)));
        } else {
            for (const auto& v : displayMesh_.V)
                ext = std::max(ext, (float) std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
        }
        meshExtent_ = std::max(ext, 0.1f);
        extentPresetId_ = meshPresetId_;
    }

    if (is4DView_) { paintWireframe(g); return; }
    if (displayMesh_.numFaces() == 0)
        return;

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
    g.drawText("drag rotate - wheel zoom - click to strike - coral bumps / teal dents",
               getLocalBounds().removeFromBottom(18), juce::Justification::centred);
}

void ManifoldView::paintWireframe(juce::Graphics& g)
{
    // 3-torus has no 2D surface. Render a SUBSAMPLED lattice (a coarse grid of
    // nodes lit by curvature concentration, breathing with the conformal
    // factor) so rotation stays smooth — drawing all ~1700 nodes + thousands
    // of tet edges every frame was the lag. The sound still uses the full mesh.
    const int nx = tetMesh_.nx, ny = tetMesh_.ny, nz = tetMesh_.nz;
    const int step = std::max(1, nx / 8);  // ~8^3 display nodes
    auto vid = [&](int i, int j, int k) { return ((i % nx) * ny + (j % ny)) * nz + (k % nz); };

    auto node = [&](int i, int j, int k, juce::Point<float>& p, float& d) {
        p = project(vid(i, j, k), d);
    };

    // structural grid edges between sampled nodes, in one stroke
    juce::Path edges;
    juce::Point<float> p, q; float dd;
    for (int i = 0; i < nx; i += step)
        for (int j = 0; j < ny; j += step)
            for (int k = 0; k < nz; k += step) {
                node(i, j, k, p, dd);
                node(i + step, j, k, q, dd); edges.startNewSubPath(p); edges.lineTo(q);
                node(i, j + step, k, q, dd); edges.startNewSubPath(p); edges.lineTo(q);
                node(i, j, k + step, q, dd); edges.startNewSubPath(p); edges.lineTo(q);
            }
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.strokePath(edges, juce::PathStrokeType(0.6f));

    // nodes, far-to-near, lit by curvature concentration
    struct Node { juce::Point<float> p; float depth; int idx; };
    std::vector<Node> nodes;
    for (int i = 0; i < nx; i += step)
        for (int j = 0; j < ny; j += step)
            for (int k = 0; k < nz; k += step) {
                const int idx = vid(i, j, k);
                juce::Point<float> pp; float d;
                node(i, j, k, pp, d);
                nodes.push_back({ pp, d, idx });
            }
    float zmin = 1e9f, zmax = -1e9f;
    for (const auto& nd : nodes) { zmin = std::min(zmin, nd.depth); zmax = std::max(zmax, nd.depth); }
    const float zspan = std::max(zmax - zmin, 1e-3f);
    std::sort(nodes.begin(), nodes.end(), [](const Node& x, const Node& y) { return x.depth < y.depth; });
    for (const auto& nd : nodes) {
        const float t = (nd.depth - zmin) / zspan;
        const float heat = nd.idx < frame_.numVerts ? frame_.kDev[nd.idx] : 0.0f;
        const float r = 1.5f + 2.5f * t;
        g.setColour(curvatureColour(heat * 3.0f, 0.45f + 0.55f * t).withAlpha(0.4f + 0.6f * t));
        g.fillEllipse(nd.p.x - r, nd.p.y - r, 2 * r, 2 * r);
    }

    if (frame_.strikeVertex < tetMesh_.numVertices()) {
        float d;
        const auto sp = project(frame_.strikeVertex, d);
        g.setColour(juce::Colour(0xFF7F77DD));
        g.fillEllipse(sp.x - 6, sp.y - 6, 12, 12);
        g.setColour(juce::Colour(0xFFEEEDFE));
        g.drawEllipse(sp.x - 6, sp.y - 6, 12, 12, 1.5f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(12.0f);
    g.drawText("3-torus (4D) - drag rotate - wheel zoom - click a node to strike",
               getLocalBounds().removeFromBottom(18), juce::Justification::centred);
}

void ManifoldView::mouseWheelMove(const juce::MouseEvent&,
                                  const juce::MouseWheelDetails& wheel)
{
    zoom_ = juce::jlimit(0.25f, 5.0f, zoom_ * std::exp(0.6f * wheel.deltaY));
    repaint();
}

void ManifoldView::mouseDown(const juce::MouseEvent& e)
{
    lastDrag_ = e.position;
}

void ManifoldView::mouseDrag(const juce::MouseEvent& e)
{
    const auto d = e.position - lastDrag_;
    yaw_ += d.x * 0.01f;
    pitch_ = juce::jlimit(-1.4f, 1.4f, pitch_ + d.y * 0.01f);
    lastDrag_ = e.position;
    repaint();
}

void ManifoldView::mouseUp(const juce::MouseEvent& e)
{
    // total travel since mouse-down decides click vs drag — per-event deltas
    // misfire on trackpads, which made strike picking feel unreliable
    if (e.getDistanceFromDragStart() > 6 || meshPresetId_ < 0)
        return;
    const int nv = is4DView_ ? tetMesh_.numVertices() : displayMesh_.numVertices();
    int best = -1;
    float bestDist = 60.0f * 60.0f;
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

SpectrumView::SpectrumView(CurvSynthProcessor& proc) : proc_(proc) {}

void SpectrumView::pushFrame(const VizFrame& f)
{
    numModes_ = f.numModes;
    curvErr_ = f.curvatureErr;
    if (histLen_ == kHist)
        std::memmove(hist_[0], hist_[1], sizeof(float) * kMaxModes * (kHist - 1));
    else
        ++histLen_;
    std::memcpy(hist_[histLen_ - 1], f.ratio, sizeof(float) * kMaxModes);
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

    // comb damping kills every other mode on the audio side; spectral warp
    // bends the ratios. Both are audio-side, so mirror them here to keep the
    // view honest about what you hear.
    const float comb = proc_.apvts.getRawParameterValue("comb")->load();
    const float combFreq = proc_.apvts.getRawParameterValue("combfreq")->load();
    const float combPeriod = 2.0f + 14.0f * (1.0f - combFreq);
    const float warp = proc_.apvts.getRawParameterValue("warp")->load();

    juce::Path path;
    for (int m = 0; m < numModes_; ++m) {
        path.clear();
        for (int i = 0; i < histLen_; ++i) {
            const float x = 26.0f + (w - 26.0f) * (float) i / (float) (kHist - 1);
            const float y = fy(std::pow(hist_[i][m], warp));
            i == 0 ? path.startNewSubPath(x, y) : path.lineTo(x, y);
        }
        float alpha = m == 0 ? 0.95f : 0.45f;
        if (comb > 0.0f) {
            const float kill = 0.5f - 0.5f * std::cos(2.0f * (float) M_PI * m / combPeriod);
            alpha *= 1.0f - 0.96f * comb * kill;
        }
        if (alpha < 0.02f)
            continue;
        g.setColour(juce::Colour(0xFF7F77DD).withAlpha(alpha));
        g.strokePath(path, juce::PathStrokeType(m == 0 ? 2.0f : 1.2f));
    }

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);
    g.drawText("partials f_k / f_1    max|K-Kbar| = " + juce::String(curvErr_, 3),
               getLocalBounds().removeFromTop(16), juce::Justification::centredRight);

    // output level meter along the bottom: green clean, amber approaching the
    // saturator, red into the crunch — so headroom is visible, not guessed
    const float peak = proc_.outputPeak();
    const float db = juce::Decibels::gainToDecibels(peak, -48.0f);
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 36.0f) / 39.0f);  // -36..+3 dBFS
    const float my = h - 5.0f, mx0 = 26.0f, mw = w - 26.0f;
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillRect(mx0, my, mw, 4.0f);
    const juce::Colour zone = db > 0.0f ? juce::Colour(0xFFE24B4A)
                            : db > -6.0f ? juce::Colour(0xFFEF9F27)
                                         : juce::Colour(0xFF1D9E75);
    g.setColour(zone);
    g.fillRect(mx0, my, mw * norm, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.fillRect(mx0 + mw * (36.0f / 39.0f), my - 1.0f, 1.0f, 6.0f);  // 0 dBFS tick
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
    addAndMakeVisible(resetButton_);
    resetAtt_ = std::make_unique<BA>(proc_.apvts, "reset", resetButton_);

    addAndMakeVisible(copyButton_);
    copyButton_.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(buildStateReport());
    };

    for (auto [id, name, suffix] : {
             std::tuple { "strike", "Strike", "" }, { "modes", "Modes", "" },
             { "mallet", "Mallet", " Hz" }, { "impulse", "Impulse", "" },
             { "bow", "Bow", "" }, { "t60", "T60", " s" },
             { "tilt", "Tilt", "" }, { "release", "Release", " s" },
             { "warp", "Spec Warp", "" }, { "comb", "Comb", "" },
             { "combfreq", "Comb Freq", "" },
             { "flowrate", "Flow Rate", "" }, { "sharpness", "Sharpness", "" },
             { "press", "Press", "" }, { "presssize", "Press Size", "" },
             { "strikedeform", "Strike Kick", "" }, { "strikeripple", "Strike Ripple", "" },
             { "ripplespeed", "Ripple Speed", "" },
             { "morphrate", "Morph Rate", "" }, { "morphdepth", "Morph Depth", "" },
             { "memory", "Memory", "" }, { "memrate", "Mem Rate", "" },
             { "gain", "Gain", " dB" } })
        addSlider(id, name, suffix);

    setSize(960, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1600, 1000);
    startTimerHz(30);
}

void CurvSynthEditor::timerCallback()
{
    // single read of the shared bus, then fan out to both views — fixes the
    // two-consumer frame stealing (press visible audibly but not in the mesh,
    // intermittent spectrum staleness)
    const VizFrame* latest = nullptr;
    proc_.vizBus().readLatest(&latest);
    if (latest != nullptr) {
        // only repaint the (expensive) manifold view when geometry actually
        // changed — avoids redundant full re-renders when the object is static
        if (latest->frameId != lastVizId_) {
            manifold_.setFrame(*latest);
            lastVizId_ = latest->frameId;
        }
        spectrum_.pushFrame(*latest);  // cheap; scrolls continuously for the time axis
    }
}

void CurvSynthEditor::addSlider(const juce::String& paramId, const juce::String& label,
                                const juce::String& suffix)
{
    auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                 juce::Slider::TextBoxRight);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 16);
    slider->setTextValueSuffix(suffix);
    slider->setNumDecimalPlacesToDisplay(paramId == "modes" ? 0 : 2);
    addAndMakeVisible(*slider);
    sliderAtts_.push_back(std::make_unique<SA>(proc_.apvts, paramId, *slider));

    auto lab = std::make_unique<juce::Label>(juce::String(), label);
    lab->setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(*lab);

    sliders_.push_back(std::move(slider));
    labels_.push_back(std::move(lab));
}

juce::String CurvSynthEditor::buildStateReport() const
{
    auto raw = [this](const char* id) { return proc_.apvts.getRawParameterValue(id)->load(); };
    auto choice = [this](const char* id) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(proc_.apvts.getParameter(id)))
            return p->getCurrentChoiceName();
        return juce::String();
    };

    const float strike = raw("strike");
    const float rate = raw("flowrate");
    const int vtx = (int) std::lround(strike * (float) std::max(manifold_.numVerts() - 1, 1));

    juce::String s;
    s << "curvsynth state report\n"
      << "manifold: " << choice("manifold") << "\n"
      << "strike: " << juce::String(strike, 3) << " (vertex " << vtx << ")\n"
      << "modes: " << (int) raw("modes") << "\n"
      << "mallet: " << juce::String(raw("mallet"), 0) << " Hz\n"
      << "t60: " << juce::String(raw("t60"), 2) << " s, tilt " << juce::String(raw("tilt"), 2)
      << ", release " << juce::String(raw("release"), 2) << " s, comb "
      << juce::String(raw("comb"), 2) << "\n"
      << "flow: " << choice("flowmode") << ", rate " << juce::String(rate, 2)
      << ", sharpness " << juce::String(raw("sharpness"), 2) << "\n"
      << "warp: " << juce::String(raw("warp"), 2) << " (1 = physical), mem rate "
      << juce::String(raw("memrate"), 2) << "\n"
      << "voices: " << choice("voicemode") << ", impulse " << juce::String(raw("impulse"), 2)
      << ", bow " << juce::String(raw("bow"), 2) << "\n"
      << "press: " << juce::String(raw("press"), 2) << ", size "
      << juce::String(raw("presssize"), 2) << " (sigma "
      << juce::String(0.8f + 5.2f * raw("presssize"), 2) << " hops)\n"
      << "strike kick: " << juce::String(raw("strikedeform"), 2)
      << ", ripple " << juce::String(raw("strikeripple"), 2)
      << ", memory " << juce::String(raw("memory"), 2) << "\n"
      << "morph: rate " << juce::String(raw("morphrate"), 2)
      << ", depth " << juce::String(raw("morphdepth"), 2) << "\n"
      << "gain: " << juce::String(raw("gain"), 1) << " dB\n"
      << "live curvature error max|K-Kbar|: " << juce::String(manifold_.curvatureErr(), 4) << "\n";
    return s;
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
    const int rowW = row.getWidth();
    manifoldBox_.setBounds(row.removeFromLeft(rowW * 26 / 100).reduced(2));
    flowBox_.setBounds(row.removeFromLeft(rowW * 16 / 100).reduced(2));
    voiceBox_.setBounds(row.removeFromLeft(rowW * 20 / 100).reduced(2));
    kickButton_.setBounds(row.removeFromLeft(rowW * 11 / 100).reduced(2));
    resetButton_.setBounds(row.removeFromLeft(rowW * 12 / 100).reduced(2));
    copyButton_.setBounds(row.reduced(2));

    const int rowH = juce::jmax(20, right.getHeight() / juce::jmax(1, (int) sliders_.size()));
    for (size_t i = 0; i < sliders_.size(); ++i) {
        auto r = right.removeFromTop(rowH);
        labels_[i]->setBounds(r.removeFromLeft(70));
        sliders_[i]->setBounds(r);
    }
}

} // namespace curv
