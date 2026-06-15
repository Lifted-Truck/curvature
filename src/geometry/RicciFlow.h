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

    // strike kick: a gentle, mean-free, smooth-random deformation localized
    // near a vertex (varied per seed) — temporary random lumps the strike
    // stamps into the metric; healed per Memory. Scale-neutral so it
    // self-corrects rather than accumulating.
    void strikeKick(int vertex, double amount, unsigned seed);

    // press gesture: continuous localized curvature injection around a
    // vertex (a localized SHARPEN — forward flow heals it on release).
    // Profile is a smooth mean-free bump over graph distance with falloff
    // radius sigma (hops); clamps still apply, and the amount halves until
    // the metric accepts it (a stressed metric takes a smaller press, never
    // silently none).
    void press(int vertex, double amount, double dt, double sigma = 2.2);

    void reset() { u_ = u0_; ripple_.setZero(); rippleVel_.setZero();
                   morph_.setZero(); morphPhase_ = 0.0; }

    // elastic restoring step toward the base metric (the Memory control's
    // engine: rate 0 = full patina, rate -> 1 = snap back)
    void relaxToBase(double rate);

    // ---- ripple: a transient wave on the conformal factor, ON TOP of the
    // (flow/press/memory-managed) base metric. A strike injects a localized
    // displacement that propagates across the surface and damps out — heard
    // as the spectrum shimmering after the hit, seen as a spreading ring.
    void rippleStrike(int vertex, double amount);
    void rippleStep(double dt, double speed, double damp);
    bool rippleActive() const { return rippleEnergy_ > 1e-9; }

    const Eigen::VectorXd& logRadii() const { return u_; }
    const Eigen::VectorXd& logRadiiBase() const { return u0_; }
    const Eigen::VectorXd& rippleField() const { return ripple_; }
    const Eigen::VectorXd& morphField() const { return morph_; }

    // ---- morph: a conformal wave that perpetually travels across the
    // manifold (set theta = base phase field; advance the phase each tick).
    // The metric never settles -> the spectrum continuously morphs.
    void setMorphField(const Eigen::VectorXd& theta) { morphTheta_ = theta; }
    void morphAdvance(double dPhase, double amp);
    bool morphActive() const { return morphAmp_ > 1e-9; }
    Eigen::VectorXd curvatureDeviation() const
    {
        return curvatures(u_).array() - kTarget_;
    }

    double curvatureError() const;   // max |K_i - Kbar| (peak; can hop vertices)
    double curvatureRms() const;     // RMS |K_i - Kbar| (smooth servo variable)

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
    std::vector<std::vector<int>> adjacency_;   // vertex -> neighbour vertices
    std::vector<std::array<int, 3>> faceEdge_;  // face corner -> edge index (opposite)
    Eigen::VectorXd invDist_;                   // per edge
    Eigen::VectorXd u0_, u_;

    int pressVertex_ = -1;
    double pressSigma_ = -1.0;
    Eigen::VectorXd pressProfile_;  // cached bump for current press vertex+sigma

    Eigen::VectorXd ripple_, rippleVel_;  // transient wave field + its velocity
    double rippleEnergy_ = 0.0;

    Eigen::VectorXd morphTheta_, morph_;  // travelling-wave phase field + current field
    double morphPhase_ = 0.0, morphAmp_ = 0.0;
};

} // namespace curv
