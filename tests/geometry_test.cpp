#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <fstream>

#include "../src/engine/GeometryService.h"
#include "../src/geometry/EigenSolver.h"
#include "../src/geometry/Presets.h"

using namespace curv;

namespace {

std::vector<double> loadReference(const std::string& name)
{
    std::ifstream f(std::string(CURV_TEST_DATA_DIR) + "/spectra/" + name + ".txt");
    REQUIRE(f.good());
    std::vector<double> vals;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty() && line[0] != '#')
            vals.push_back(std::stod(line));
    return vals;
}

std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    REQUIRE(f.good());
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

void checkAgainstPython(const Mesh& mesh, const std::string& refName, double rtol)
{
    const auto ref = loadReference(refName);
    const auto modes = solveModes(buildCotanLaplacian(mesh), (int) ref.size());
    for (size_t i = 0; i < ref.size(); ++i) {
        const double rel = std::abs(modes.lambda[(Eigen::Index) i] - ref[i]) / ref[i];
        INFO(refName << " mode " << i);
        REQUIRE(rel < rtol);
    }
}

} // namespace

TEST_CASE("topology: Euler characteristic per preset")
{
    REQUIRE(makeIcosphere(2).genus() == 0);
    REQUIRE(makeFlatTorus(24, 18, 1.0, 1.0).genus() == 1);
}

TEST_CASE("flat torus: plane waves are exact discrete eigenvectors")
{
    const int nx = 12, ny = 8;
    const auto mesh = makeFlatTorus(nx, ny, 1.0, 1.618);
    const auto lap = buildCotanLaplacian(mesh);

    for (auto [m, nn] : { std::pair { 1, 0 }, { 0, 1 }, { 2, 3 }, { 5, 2 } }) {
        Eigen::VectorXcd v(mesh.numVertices());
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
                v[i * ny + j] = std::exp(std::complex<double>(
                    0.0, 2.0 * M_PI * (m * (double) i / nx + nn * (double) j / ny)));

        const Eigen::VectorXcd Lv = lap.L.cast<std::complex<double>>() * v;
        const Eigen::VectorXcd Mv = lap.massDiag.cast<std::complex<double>>().asDiagonal() * v;
        const double lam = (v.dot(Lv) / v.dot(Mv)).real();
        const double residual = (Lv - lam * Mv).norm() / Lv.norm();
        REQUIRE(residual < 1e-9);
    }
}

TEST_CASE("eigenvalues match Phase 0 Python within tolerance")
{
    checkAgainstPython(makeIcosphere(3), "icosphere3", 1e-6);
    checkAgainstPython(makeFlatTorus(32, 24, 1.0, 1.0), "torus_1x1", 1e-8);
    checkAgainstPython(makeFlatTorus(32, 24, 1.0, 1.618), "torus_1x1p618", 1e-8);
    checkAgainstPython(makeFlatTorus(96, 12, 8.0, 1.0), "torus_8x1", 1e-8);
    checkAgainstPython(loadObjFromString(readFile(std::string(CURV_ASSETS_DIR) + "/manifolds/genus2.obj").c_str(),
                                         readFile(std::string(CURV_ASSETS_DIR) + "/manifolds/genus2.obj").size(),
                                         "genus2"),
                       "genus2", 1e-6);
}

TEST_CASE("8:1 torus produces a harmonic series (geometry-as-harmonicity)")
{
    const auto modes = solveModes(buildCotanLaplacian(makeFlatTorus(96, 12, 8.0, 1.0)), 14);
    for (int harmonic = 1; harmonic <= 7; ++harmonic) {
        const double ratio = std::sqrt(modes.lambda[2 * (harmonic - 1)] / modes.lambda[0]);
        REQUIRE_THAT(ratio, Catch::Matchers::WithinRel((double) harmonic, 0.01));
    }
}

TEST_CASE("spectrum frame: ratios ascend from 1, coupling normalized")
{
    GeometryService geo;
    geo.loadPreset(PresetId::Icosphere, nullptr, 0);
    SpectrumFrame frame;
    geo.fillFrame(frame, 96, 0.42f);

    REQUIRE(frame.numModes == 96);
    REQUIRE_THAT((double) frame.ratio[0], Catch::Matchers::WithinAbs(1.0, 1e-6));
    float maxCoupling = 0.0f;
    for (int m = 1; m < frame.numModes; ++m) {
        REQUIRE(frame.ratio[m] >= frame.ratio[m - 1] - 1e-6f);
        maxCoupling = std::max(maxCoupling, std::abs(frame.coupling[m]));
    }
    REQUIRE_THAT((double) maxCoupling, Catch::Matchers::WithinAbs(1.0, 1e-5));
}
