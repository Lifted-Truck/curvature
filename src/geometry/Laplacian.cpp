#include "Laplacian.h"

#include <cmath>
#include <stdexcept>

namespace curv {

namespace {
constexpr double kCotClamp = 1e4;
}

LaplacianPair buildCotanLaplacian(const Mesh& mesh)
{
    const int n = mesh.numVertices();
    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(mesh.F.size() * 12);
    Eigen::VectorXd mass = Eigen::VectorXd::Zero(n);

    for (size_t fi = 0; fi < mesh.F.size(); ++fi) {
        const auto& f = mesh.F[fi];
        const auto& l = mesh.faceLengths[fi];
        const double l0 = l[0], l1 = l[1], l2 = l[2];

        const double s = 0.5 * (l0 + l1 + l2);
        const double areaSq = s * (s - l0) * (s - l1) * (s - l2);
        if (areaSq <= 0.0)
            throw std::runtime_error(mesh.name + ": face violates triangle inequality");
        const double area = std::sqrt(areaSq);

        // cot(angle at corner j) = (adjacent^2 sum - opposite^2) / (4 area)
        const double cot[3] = {
            std::clamp((l1 * l1 + l2 * l2 - l0 * l0) / (4.0 * area), -kCotClamp, kCotClamp),
            std::clamp((l2 * l2 + l0 * l0 - l1 * l1) / (4.0 * area), -kCotClamp, kCotClamp),
            std::clamp((l0 * l0 + l1 * l1 - l2 * l2) / (4.0 * area), -kCotClamp, kCotClamp),
        };

        static constexpr int opp[3][2] = { { 1, 2 }, { 2, 0 }, { 0, 1 } };
        for (int j = 0; j < 3; ++j) {
            const int a = f[opp[j][0]], b = f[opp[j][1]];
            const double w = 0.5 * cot[j];
            trips.emplace_back(a, b, -w);
            trips.emplace_back(b, a, -w);
            trips.emplace_back(a, a, w);
            trips.emplace_back(b, b, w);
        }
        for (int j = 0; j < 3; ++j)
            mass[f[j]] += area / 3.0;
    }

    LaplacianPair out;
    out.L.resize(n, n);
    out.L.setFromTriplets(trips.begin(), trips.end());
    out.massDiag = std::move(mass);
    return out;
}

} // namespace curv
