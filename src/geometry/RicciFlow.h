// Chow-Luo combinatorial Ricci flow on an inversive-distance circle packing,
// ported 1:1 from prototype/ricci.py (the validated reference).
//
// Metric state: per-vertex log radii u plus per-edge inversive distances
// fixed by the initial metric. Forward flow drives curvature toward the
// uniform Gauss-Bonnet target (the topological attractor); reverse flow is
// the same field negated under hard |u - u0| clamps. Explicit Euler with
// step-halving on metric validity and (forward only) on curvature-error
// monotonicity — the stability criterion doubles as the Phase 2 oracle.
//
// Runs on the geometry thread only; everything here may allocate.
#pragma once

#include <Eigen/Dense>

#include "Mesh.h"

namespace curv {

class RicciFlow {
public:
    explicit RicciFlow(const Mesh& mesh, double uClamp = 1.5);

    // smooth random conformal kick (test/start state for RELAX)
    void perturb(double amplitude, unsigned seed);

    // one Euler step; direction +1 = RELAX, -1 = SHARPEN; returns dt taken
    double step(double dt, double direction);

    // press gesture: continuous localized curvature injection around a
    // vertex (a localized SHARPEN — forward flow heals it on release).
    // Profile is a smooth bump over graph distance; clamps still apply.
    void press(int vertex, double amount, double dt);

    void reset() { u_ = u0_; }

    const Eigen::VectorXd& logRadii() const { return u_; }
    const Eigen::VectorXd& logRadiiBase() const { return u0_; }
    Eigen::VectorXd curvatureDeviation() const
    {
        return curvatures(u_).array() - kTarget_;
    }

    double curvatureError() const;

    // write the current metric into a mesh's faceLengths
    void writeFaceLengths(Mesh& mesh) const;

private:
    Eigen::VectorXd curvatures(const Eigen::VectorXd& u) const;
    void faceLengthsFor(const Eigen::VectorXd& u,
                        std::vector<std::array<double, 3>>& out) const;
    bool isValid(const Eigen::VectorXd& u) const;

    const Mesh* mesh_;
    double uClamp_;
    double kTarget_ = 0.0;

    std::vector<std::array<int, 2>> edges_;
    std::vector<std::array<int, 3>> faceEdge_;  // face corner -> edge index (opposite)
    Eigen::VectorXd invDist_;                   // per edge
    Eigen::VectorXd u0_, u_;

    int pressVertex_ = -1;
    Eigen::VectorXd pressProfile_;  // cached bump for the current press vertex
};

} // namespace curv
