// Index-based triangle mesh. Vertex positions are for display and
// strike-point picking only; the metric state is faceLengths (per-face edge
// lengths, column j opposite corner j), per the all-phases invariant in
// CLAUDE.md. Pure C++ — no JUCE.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace curv {

struct Mesh {
    std::string name;
    std::vector<std::array<double, 3>> V;
    std::vector<std::array<int, 3>> F;
    std::vector<std::array<double, 3>> faceLengths;  // [j] = length of edge opposite corner j

    int numVertices() const { return static_cast<int>(V.size()); }
    int numFaces() const { return static_cast<int>(F.size()); }

    // derive faceLengths from the embedding (presets with intrinsic metrics
    // override instead of calling this)
    void computeLengthsFromPositions();

    int eulerCharacteristic() const;
    int genus() const;
};

} // namespace curv
