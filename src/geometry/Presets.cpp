#include "Presets.h"

#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>

namespace curv {

namespace {

void subdivideOnce(std::vector<std::array<double, 3>>& V, std::vector<std::array<int, 3>>& F)
{
    std::map<std::pair<int, int>, int> midpointCache;
    auto midpoint = [&](int a, int b) {
        const auto key = std::pair { std::min(a, b), std::max(a, b) };
        auto it = midpointCache.find(key);
        if (it != midpointCache.end())
            return it->second;
        V.push_back({ (V[(size_t) a][0] + V[(size_t) b][0]) / 2.0,
                      (V[(size_t) a][1] + V[(size_t) b][1]) / 2.0,
                      (V[(size_t) a][2] + V[(size_t) b][2]) / 2.0 });
        const int idx = static_cast<int>(V.size()) - 1;
        midpointCache.emplace(key, idx);
        return idx;
    };

    std::vector<std::array<int, 3>> out;
    out.reserve(F.size() * 4);
    for (const auto& [a, b, c] : F) {
        const int ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
        out.push_back({ a, ab, ca });
        out.push_back({ b, bc, ab });
        out.push_back({ c, ca, bc });
        out.push_back({ ab, bc, ca });
    }
    F = std::move(out);
}

} // namespace

Mesh makeIcosphere(int subdiv, double radius)
{
    const double t = (1.0 + std::sqrt(5.0)) / 2.0;
    Mesh m;
    m.name = "icosphere" + std::to_string(subdiv);
    m.V = { { -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
            { 0, -1, t }, { 0, 1, t }, { 0, -1, -t }, { 0, 1, -t },
            { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 } };
    m.F = { { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
            { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
            { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
            { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };

    for (int i = 0; i < subdiv; ++i)
        subdivideOnce(m.V, m.F);

    for (auto& v : m.V) {
        const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        for (auto& x : v)
            x *= radius / n;
    }
    m.computeLengthsFromPositions();
    return m;
}

Mesh makeFlatTorus(int nx, int ny, double a, double b)
{
    const double dx = a / nx, dy = b / ny;
    Mesh m;
    m.name = "torus_lattice";
    auto vid = [&](int i, int j) { return ((i % nx + nx) % nx) * ny + ((j % ny + ny) % ny); };

    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j) {
            const double theta = 2.0 * M_PI * i / nx, phi = 2.0 * M_PI * j / ny;
            const double R = 1.0, r = 0.4;  // display donut only
            m.V.push_back({ (R + r * std::cos(phi)) * std::cos(theta),
                            (R + r * std::cos(phi)) * std::sin(theta),
                            r * std::sin(phi) });
        }

    const double diag = std::hypot(dx, dy);
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j) {
            m.F.push_back({ vid(i, j), vid(i + 1, j), vid(i + 1, j + 1) });
            m.faceLengths.push_back({ dy, diag, dx });
            m.F.push_back({ vid(i, j), vid(i + 1, j + 1), vid(i, j + 1) });
            m.faceLengths.push_back({ dx, dy, diag });
        }
    return m;
}

Mesh loadObjFromString(const char* data, size_t size, const std::string& name)
{
    Mesh m;
    m.name = name;
    std::istringstream in(std::string(data, size));
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "v") {
            std::array<double, 3> v {};
            ls >> v[0] >> v[1] >> v[2];
            m.V.push_back(v);
        } else if (tag == "f") {
            std::array<int, 3> f {};
            ls >> f[0] >> f[1] >> f[2];
            m.F.push_back({ f[0] - 1, f[1] - 1, f[2] - 1 });
        }
    }
    if (m.V.empty() || m.F.empty())
        throw std::runtime_error("OBJ parse produced empty mesh: " + name);
    m.computeLengthsFromPositions();
    return m;
}

Mesh makePreset(PresetId id, const char* obj, size_t objSize)
{
    switch (id) {
        case PresetId::Icosphere:   return makeIcosphere(3);
        case PresetId::Torus11:     return makeFlatTorus(32, 24, 1.0, 1.0);
        case PresetId::TorusGolden: return makeFlatTorus(32, 24, 1.0, 1.618);
        case PresetId::TorusString: return makeFlatTorus(96, 12, 8.0, 1.0);
        case PresetId::Genus2:      return loadObjFromString(obj, objSize, "genus2");
        case PresetId::Mandelbulb:  return loadObjFromString(obj, objSize, "mandelbulb");
        default:                    throw std::runtime_error("bad preset id");
    }
}

} // namespace curv
