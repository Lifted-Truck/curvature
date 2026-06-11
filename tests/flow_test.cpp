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
