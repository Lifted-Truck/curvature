#include "Mesh.h"

#include <cmath>
#include <set>
#include <stdexcept>

namespace curv {

void Mesh::computeLengthsFromPositions()
{
    faceLengths.resize(F.size());
    auto dist = [this](int a, int b) {
        const auto &p = V[(size_t) a], &q = V[(size_t) b];
        const double dx = p[0] - q[0], dy = p[1] - q[1], dz = p[2] - q[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    for (size_t i = 0; i < F.size(); ++i) {
        const auto& f = F[i];
        faceLengths[i] = { dist(f[1], f[2]), dist(f[2], f[0]), dist(f[0], f[1]) };
    }
}

int Mesh::eulerCharacteristic() const
{
    std::set<std::pair<int, int>> edges;
    for (const auto& f : F)
        for (auto [a, b] : { std::pair { f[0], f[1] }, { f[1], f[2] }, { f[2], f[0] } })
            edges.insert({ std::min(a, b), std::max(a, b) });
    return numVertices() - static_cast<int>(edges.size()) + numFaces();
}

int Mesh::genus() const
{
    const int chi = eulerCharacteristic();
    if (chi % 2 != 0)
        throw std::runtime_error(name + ": odd Euler characteristic, not a closed surface");
    return (2 - chi) / 2;
}

} // namespace curv
