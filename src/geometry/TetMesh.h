// Tetrahedral mesh of a flat 3-manifold (the 3-torus T^3) and its 3D
// finite-element Laplacian — the "4D object". Mirrors prototype/manifold4d.py
// (Freudenthal 6-tet split of a periodic cube; FEM stiffness from barycentric
// gradients, lumped mass). The metric is intrinsic: per-vertex conformal
// factors scale edge lengths, so the same press/kick/ripple/relax gestures
// apply one dimension up.
#pragma once

#include <array>
#include <vector>

#include <Eigen/Sparse>

#include "Laplacian.h"  // LaplacianPair

namespace curv {

struct TetMesh {
    int nx = 0, ny = 0, nz = 0;
    double a = 1.0, b = 1.0, c = 1.0;          // bounding extents (display)
    double lat[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };  // period basis (columns)
    std::vector<std::array<double, 3>> V;      // fundamental-domain positions (display)
    std::vector<std::array<int, 4>> tets;      // periodic vertex indices
    std::vector<std::array<int, 2>> edges;     // unique edges (display wireframe + smoothing)
    std::vector<std::vector<int>> adjacency;   // vertex -> neighbours
    std::vector<double> conformal;             // baked base conformal factor (empty = flat)

    int numVertices() const { return static_cast<int>(V.size()); }
};

// orthogonal lattice (period a x b x c)
TetMesh makeFlatTorus3(int nx, int ny, int nz, double a, double b, double c);

// general (possibly oblique) lattice: columns of basis are the period vectors
TetMesh makeLatticeTorus3(int nx, int ny, int nz, const double basis[3][3]);

// flat 3-torus with a fixed deterministic smooth periodic vertex displacement
// -> a generic (chaotic) metric: GOE level statistics, dense irregular shimmer
// (a tractable proxy for a hyperbolic 3-manifold). Matches prototype/hyperbolic.py.
TetMesh makeChaoticTorus3(int nx, int ny, int nz, double amp);

// FEM Laplacian (PSD, L*1 = 0) + lumped mass for the given per-vertex
// conformal factor u (edge geometry scaled by exp((u_i+u_j)/2)); u empty => flat.
LaplacianPair buildTetLaplacian(const TetMesh& mesh, const Eigen::VectorXd& u);

} // namespace curv
