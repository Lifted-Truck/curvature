// Phase 2 soak gate: 30 simulated minutes of flow at geometry-thread cadence
// (25 ms steps, ~1 Hz re-solves), alternating RELAX / SHARPEN / kick cycles.
// Checks every frame for NaN/Inf/denormal eigenvalues and divergence.
// Runs as fast as the CPU allows (no realtime sleeps).
//
// Usage: flow_soak [minutes] [preset] [genus2-obj-path]
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "../src/engine/GeometryService.h"

int main(int argc, char** argv)
{
    const double minutes = argc > 1 ? std::stod(argv[1]) : 30.0;
    const int preset = argc > 2 ? std::stoi(argv[2]) : (int) curv::PresetId::TorusGolden;
    const std::string objPath = argc > 3 ? argv[3] : "assets/manifolds/genus2.obj";

    std::string objData;
    if (preset == (int) curv::PresetId::Genus2) {
        std::ifstream f(objPath);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", objPath.c_str()); return 1; }
        objData.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    curv::GeometryService geo;
    geo.loadPreset((curv::PresetId) preset, objData.data(), objData.size());

    const long steps = (long) (minutes * 60.0 / 0.025);  // 25 ms cadence
    const long resolveEvery = 40;                        // ~1 Hz
    const long phaseLen = 30 * 40;                       // 30 s per flow phase
    curv::SpectrumFrame frame;
    unsigned seed = 1;
    double maxRatioSeen = 0.0;

    for (long i = 0; i < steps; ++i) {
        const int phase = (int) ((i / phaseLen) % 4);  // relax, sharpen, relax, kick+relax
        if (phase == 3 && i % phaseLen == 0)
            geo.flowKick(0.6, seed++);
        geo.flowStep(0.1, phase == 1 ? -1.0 : +1.0);
        if (i % resolveEvery == resolveEvery - 1)
            geo.resolve();

        geo.fillFrame(frame, 96, 0.37f);
        const double kErr = geo.curvatureError();
        if (!std::isfinite(kErr)) { std::fprintf(stderr, "FAIL: curvature NaN at step %ld\n", i); return 1; }
        for (int m = 0; m < frame.numModes; ++m) {
            const float r = frame.ratio[m];
            if (!std::isfinite(r) || (r != 0.0f && std::abs(r) < 1e-30f)) {
                std::fprintf(stderr, "FAIL: ratio[%d] = %g at step %ld\n", m, (double) r, i);
                return 1;
            }
            maxRatioSeen = std::max(maxRatioSeen, (double) r);
        }
        if (maxRatioSeen > 1e4) { std::fprintf(stderr, "FAIL: spectrum diverged at step %ld\n", i); return 1; }

        if (i % (steps / 20) == 0)
            std::printf("  %2ld%%  step %ld  K_err %.4f  ratio_max %.1f\n",
                        i * 100 / steps, i, kErr, maxRatioSeen);
    }
    std::printf("PASS: %.0f min simulated, no NaN/denormal/divergence (ratio_max %.1f)\n",
                minutes, maxRatioSeen);
    return 0;
}
