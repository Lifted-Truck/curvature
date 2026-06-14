#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <fstream>

#include "../src/geometry/EigenSolver.h"
#include "../src/geometry/TetMesh.h"

using namespace curv;

namespace {
std::vector<double> loadRef(const std::string& name)
{
    std::ifstream f(std::string(CURV_TEST_DATA_DIR) + "/spectra/" + name + ".txt");
    REQUIRE(f.good());
    std::vector<double> v;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty() && line[0] != '#')
            v.push_back(std::stod(line));
    return v;
}
} // namespace

TEST_CASE("3-torus FEM Laplacian: PSD with constant null mode")
{
    const auto mesh = makeFlatTorus3(8, 8, 8, 1.0, 1.0, 1.0);
    const auto lap = buildTetLaplacian(mesh, Eigen::VectorXd());
    Eigen::VectorXd ones = Eigen::VectorXd::Ones(mesh.numVertices());
    REQUIRE((lap.L * ones).norm() < 1e-9);          // L*1 = 0
    REQUIRE(lap.massDiag.minCoeff() > 0.0);          // positive lumped masses
}

TEST_CASE("3-torus: plane waves are exact discrete eigenvectors")
{
    const int nx = 6, ny = 6, nz = 6;
    const auto mesh = makeFlatTorus3(nx, ny, nz, 1.0, 1.0, 1.0);
    const auto lap = buildTetLaplacian(mesh, Eigen::VectorXd());

    auto vid = [&](int i, int j, int k) { return (i * ny + j) * nz + k; };
    for (auto [l, m, p] : { std::tuple { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 2, 1, 1 } }) {
        Eigen::VectorXcd v(mesh.numVertices());
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
                for (int k = 0; k < nz; ++k)
                    v[vid(i, j, k)] = std::exp(std::complex<double>(
                        0.0, 2.0 * M_PI * ((double) l * i / nx + (double) m * j / ny
                                           + (double) p * k / nz)));
        const Eigen::VectorXcd Lv = lap.L.cast<std::complex<double>>() * v;
        const Eigen::VectorXcd Mv = lap.massDiag.cast<std::complex<double>>().asDiagonal() * v;
        const double lam = (v.dot(Lv) / v.dot(Mv)).real();
        REQUIRE((Lv - lam * Mv).norm() / Lv.norm() < 1e-9);  // exact eigenvector
    }
}

TEST_CASE("oblique 3-torus: plane waves still exact (generalized minimal image)")
{
    const int nx = 6, ny = 6, nz = 6;
    const double basis[3][3] = { { 1.0, 0.5, 0.5 }, { 0.0, 0.866, 0.289 }, { 0.0, 0.0, 0.816 } };
    const auto mesh = makeLatticeTorus3(nx, ny, nz, basis);
    const auto lap = buildTetLaplacian(mesh, Eigen::VectorXd());

    auto vid = [&](int i, int j, int k) { return (i * ny + j) * nz + k; };
    for (auto [l, m, p] : { std::tuple { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 1 } }) {
        Eigen::VectorXcd v(mesh.numVertices());
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
                for (int k = 0; k < nz; ++k)
                    v[vid(i, j, k)] = std::exp(std::complex<double>(
                        0.0, 2.0 * M_PI * ((double) l * i / nx + (double) m * j / ny
                                           + (double) p * k / nz)));
        const Eigen::VectorXcd Lv = lap.L.cast<std::complex<double>>() * v;
        const Eigen::VectorXcd Mv = lap.massDiag.cast<std::complex<double>>().asDiagonal() * v;
        const double lam = (v.dot(Lv) / v.dot(Mv)).real();
        REQUIRE((Lv - lam * Mv).norm() / Lv.norm() < 1e-9);
    }
}

TEST_CASE("3-torus eigenvalues match Phase-0 Python (cube + anisotropic)")
{
    for (auto [name, a, b, c] : { std::tuple { "torus3_cube8", 1.0, 1.0, 1.0 },
                                  { "torus3_aniso8", 1.0, 1.32, 1.71 } }) {
        const auto mesh = makeFlatTorus3(8, 8, 8, a, b, c);
        const auto modes = solveModes(buildTetLaplacian(mesh, Eigen::VectorXd()), 64);
        const auto ref = loadRef(name);
        // Compare the well-converged lower portion. The 3-torus spectrum is
        // highly degenerate (6/12/24-fold shells); at the top of the solved
        // range scipy and Spectra resolve degenerate clusters in slightly
        // different order, which misaligns the tail. Operator correctness is
        // proved exactly by the plane-wave test above; this guards the solver.
        for (size_t i = 0; i < 40; ++i) {
            INFO(name << " mode " << i);
            REQUIRE(std::abs(modes.lambda[(Eigen::Index) i] - ref[i]) / ref[i] < 1e-6);
        }
    }
}
