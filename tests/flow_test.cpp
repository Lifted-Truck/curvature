#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "../src/engine/GeometryService.h"
#include "../src/geometry/Presets.h"
#include "../src/geometry/RicciFlow.h"

using namespace curv;

TEST_CASE("forward Ricci flow: curvature error monotonically decreasing")
{
    for (auto mesh : { makeIcosphere(3), makeFlatTorus(32, 24, 1.0, 1.618) }) {
        RicciFlow flow(mesh);
        flow.perturb(0.6, 0);
        double prev = flow.curvatureError();
        const double start = prev;
        for (int i = 0; i < 300; ++i) {
            REQUIRE(flow.step(0.3, +1.0) > 0.0);
            const double err = flow.curvatureError();
            REQUIRE(err <= prev + 1e-9);
            prev = err;
        }
        REQUIRE(prev < 0.5 * start);
    }
}

TEST_CASE("reverse flow: clamps respected, metric stays valid")
{
    auto mesh = makeIcosphere(3);
    RicciFlow flow(mesh, /*uClamp=*/1.0);
    for (int i = 0; i < 400; ++i)
        flow.step(0.2, -1.0);
    // the flow object refuses invalid metrics by construction; curvature
    // error is finite and the lengths it writes back form valid triangles
    REQUIRE(std::isfinite(flow.curvatureError()));
    Mesh probe = mesh;
    flow.writeFaceLengths(probe);
    REQUIRE_NOTHROW(buildCotanLaplacian(probe));
}

TEST_CASE("eigenvalue trajectories continuous through fast path and re-solves")
{
    GeometryService geo;
    geo.loadPreset(PresetId::TorusGolden, nullptr, 0);
    geo.flowKick(0.6, 0);
    geo.resolve();  // fresh basis on the kicked metric

    SpectrumFrame frame;
    geo.fillFrame(frame, 96, 0.3f);
    float prevRatio[96];
    for (int m = 0; m < 96; ++m)
        prevRatio[m] = frame.ratio[m];

    for (int step = 0; step < 60; ++step) {
        REQUIRE(geo.flowStep(0.15, +1.0) > 0.0);
        if (step % 20 == 19)
            geo.resolve();  // scheduled re-solve mid-trajectory
        geo.fillFrame(frame, 96, 0.3f);
        for (int m = 0; m < 96; ++m) {
            REQUIRE(std::isfinite(frame.ratio[m]));
            // no frame-to-frame jump > 5% — re-solves must reconcile smoothly
            REQUIRE(std::abs(frame.ratio[m] - prevRatio[m]) <= 0.05f * prevRatio[m] + 1e-4f);
            prevRatio[m] = frame.ratio[m];
        }
    }
}

TEST_CASE("press gesture: localized curvature injection, healed by relax")
{
    auto mesh = makeIcosphere(3);
    RicciFlow flow(mesh);
    const int vtx = 100;
    for (int i = 0; i < 40; ++i)
        flow.press(vtx, 2.0, 0.05);

    const Eigen::VectorXd bump = flow.logRadii() - flow.logRadiiBase();
    REQUIRE(bump[vtx] > 0.5);                              // bump grew where pressed
    REQUIRE(bump[vtx] == bump.maxCoeff());                 // and is largest there
    int far = 0;
    for (int i = 0; i < mesh.numVertices(); ++i)
        if (bump[i] < 0.05 * bump[vtx])
            ++far;
    REQUIRE(far > mesh.numVertices() / 2);                 // localized, not global

    double err = flow.curvatureError();
    for (int i = 0; i < 400; ++i)
        flow.step(0.3, +1.0);
    REQUIRE(flow.curvatureError() < 0.2 * err);            // relax heals the press

    // press is mean-free (scale-neutral), so relax must restore the *base
    // metric*, not converge to a uniformly rescaled copy of it
    const Eigen::VectorXd residual = flow.logRadii() - flow.logRadiiBase();
    REQUIRE(residual.cwiseAbs().maxCoeff() < 0.08);
}

TEST_CASE("elastic restoring (Memory < 1) heals any deformation to base")
{
    auto mesh = makeFlatTorus(24, 18, 1.0, 1.618);
    RicciFlow flow(mesh);
    flow.perturb(0.6, 5);
    for (int i = 0; i < 30; ++i)
        flow.press(50, 2.5, 0.05, 1.5);  // pointy press into the clamps

    for (int i = 0; i < 400; ++i)
        flow.relaxToBase(0.05);
    const Eigen::VectorXd residual = flow.logRadii() - flow.logRadiiBase();
    // unlike Ricci relax, the elastic force targets u0 directly, so even
    // clamp-leaked scale comes back out
    REQUIRE(residual.cwiseAbs().maxCoeff() < 1e-4);
}

TEST_CASE("manual servo invariant: metric deviation is monotone under the flow")
{
    // The Manual-mode position control drives ||u - u0|| because it is the
    // monotone coordinate of the flow: Sharpen grows it, elastic relax
    // shrinks it. (Curvature error is NOT monotone — it's a max over
    // vertices that jumps — which is why the servo can't drive on it.)
    GeometryService geo;
    geo.loadPreset(PresetId::Icosphere, nullptr, 0);
    geo.flowKick(0.05, 1);  // symmetry-break seed

    double dev = geo.metricDeviation();
    bool grew = false;
    for (int i = 0; i < 40; ++i) {
        geo.flowStep(0.2, -1.0);  // Sharpen
        const double d = geo.metricDeviation();
        if (d > dev + 1e-6) grew = true;
        REQUIRE(d >= dev - 1e-6);   // never decreases under Sharpen
        dev = d;
    }
    REQUIRE(grew);                   // and it does increase

    for (int i = 0; i < 200; ++i) {
        geo.flowElastic(0.2);        // relax toward base
        const double d = geo.metricDeviation();
        REQUIRE(d <= dev + 1e-9);    // never increases under elastic relax
        dev = d;
    }
    REQUIRE(dev < 1e-3);             // and converges back to base
}

TEST_CASE("manual servo invariant: RMS curvature is smooth and bidirectional")
{
    // Manual mode scrubs along the flow on RMS curvature: reverse flow raises
    // it (sharpen, up), forward flow lowers it (relax, down — 'walking back
    // down the flow'). RMS is the smooth servo variable (max|K-Kbar| hops
    // between vertices and was the jitter source).
    GeometryService geo;
    geo.loadPreset(PresetId::Icosphere, nullptr, 0);
    geo.flowKick(0.05, 1);

    double rms = geo.curvatureRms();
    for (int i = 0; i < 40; ++i) {        // up: reverse flow raises RMS
        geo.flowStep(0.2, -1.0);
        const double r = geo.curvatureRms();
        REQUIRE(r >= rms - 1e-6);
        rms = r;
    }
    const double sharpened = rms;
    REQUIRE(sharpened > 0.05);            // it did sharpen

    for (int i = 0; i < 300; ++i) {       // down: forward flow lowers RMS
        geo.flowStep(0.2, +1.0);
        const double r = geo.curvatureRms();
        REQUIRE(r <= rms + 1e-6);
        rms = r;
    }
    REQUIRE(rms < 0.3 * sharpened);       // smoothing walked it back down
}

TEST_CASE("extreme sharpen + strike kicks never throw (engine coasts, no reset)")
{
    // reproduces the runaway: drive curvature concentration hard while
    // stamping strike kicks. resolve()/rayleighUpdate must swallow any
    // eigensolve/degenerate-metric failure and coast, never throw — so the
    // host-side geometry loop never has to reset the instrument.
    GeometryService geo;
    geo.loadPreset(PresetId::Icosphere, nullptr, 0);
    unsigned seed = 1;
    for (int i = 0; i < 600; ++i) {
        REQUIRE_NOTHROW(geo.flowStep(0.25, -1.0));   // hard reverse flow (sharpen)
        if (i % 5 == 0)
            REQUIRE_NOTHROW(geo.strikeKick(0.42f, 0.45, seed++));
        if (i % 20 == 19)
            REQUIRE_NOTHROW(geo.resolve());
        // whatever happens to the metric, the published spectrum stays finite
        SpectrumFrame f;
        geo.fillFrame(f, 96, 0.42f);
        for (int m = 0; m < f.numModes; ++m)
            REQUIRE(std::isfinite(f.ratio[m]));
    }
}

TEST_CASE("ripple: a strike wave propagates outward then damps to rest")
{
    auto mesh = makeIcosphere(3);
    RicciFlow flow(mesh);
    REQUIRE_FALSE(flow.rippleActive());

    flow.rippleStrike(100, 0.4, 2.0);
    REQUIRE(flow.rippleActive());           // energy injected

    bool stayedFinite = true;
    for (int i = 0; i < 2000; ++i) {
        flow.rippleStep(0.03, 6.0, 2.0);
        Mesh probe = mesh;
        flow.writeFaceLengths(probe);        // must stay a valid metric throughout
        if (!std::isfinite(flow.curvatureError())) { stayedFinite = false; break; }
    }
    REQUIRE(stayedFinite);
    REQUIRE_FALSE(flow.rippleActive());      // damping returned it to rest
}

TEST_CASE("fundamental stays the lowest under heavy morph+flow (no undertones)")
{
    // regression: mode-identity tracking can leave slot 0 non-minimal, which
    // (when ratios normalized by slot 0) collapsed the register and made Warp
    // produce undertones. Normalizing by the true minimum must keep every
    // published ratio >= 1 no matter how scrambled the mode order gets.
    GeometryService geo;
    geo.loadPreset(PresetId::TorusGolden, nullptr, 0);
    geo.flowKick(0.6, 3);
    for (int i = 0; i < 120; ++i) {
        geo.morphStep(0.2, 0.6);
        geo.flowStep(0.2, (i % 2) ? +1.0 : -1.0);
        if (i % 15 == 14) geo.resolve();
        SpectrumFrame f;
        geo.fillFrame(f, 96, 0.4f);
        float minRatio = 1e9f;
        for (int m = 0; m < f.numModes; ++m) minRatio = std::min(minRatio, f.ratio[m]);
        REQUIRE(minRatio >= 1.0f - 1e-4f);  // fundamental is always the lowest
    }
}

TEST_CASE("morph: perpetually changes the spectrum, stays finite and bounded")
{
    GeometryService geo;
    geo.loadPreset(PresetId::TorusGolden, nullptr, 0);

    SpectrumFrame f0;
    geo.fillFrame(f0, 96, 0.4f);
    std::vector<float> r0(f0.ratio, f0.ratio + f0.numModes);

    bool changed = false, finite = true;
    for (int i = 0; i < 80; ++i) {
        geo.morphStep(0.18, 0.5);          // advance the travelling wave
        SpectrumFrame f;
        geo.fillFrame(f, 96, 0.4f);
        for (int m = 0; m < f.numModes; ++m) {
            if (!std::isfinite(f.ratio[m]) || f.ratio[m] > 1e4f) finite = false;
            if (std::abs(f.ratio[m] - r0[(size_t) m]) > 0.01f) changed = true;
        }
    }
    REQUIRE(finite);     // never blows up
    REQUIRE(changed);    // the spectrum genuinely morphs
}

TEST_CASE("flow reset restores the base spectrum")
{
    GeometryService geo;
    geo.loadPreset(PresetId::Icosphere, nullptr, 0);
    const Eigen::VectorXd base = geo.lambda();

    geo.flowKick(0.5, 7);
    double kicked = 0.0;
    for (int m = 0; m < 16; ++m)
        kicked = std::max(kicked, std::abs(geo.lambda()[m] / base[m] - 1.0));
    REQUIRE(kicked > 0.01);  // the kick actually moved the spectrum

    geo.flowReset();
    for (int m = 0; m < 16; ++m)
        REQUIRE(std::abs(geo.lambda()[m] / base[m] - 1.0) < 1e-6);
}
