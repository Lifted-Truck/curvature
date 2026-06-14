// 3-manifold geometry backend (the 4D object): a conformal factor over a tet
// mesh, with the same gesture vocabulary as RicciFlow (press / strike kick /
// ripple / memory) one dimension up. The curvature-driven Ricci step is
// replaced by conformal diffusion — relax smooths the conformal factor toward
// uniform (the round 3-torus), sharpen concentrates it. Everything operates
// on the per-vertex factor u and rebuilds the FEM Laplacian, exactly mirroring
// the 2-manifold flow so GeometryService treats them the same.
#pragma once

#include <Eigen/Dense>

#include "Laplacian.h"
#include "TetMesh.h"

namespace curv {

class TetManifold {
public:
    explicit TetManifold(TetMesh mesh);

    int numVertices() const { return mesh_.numVertices(); }
    const TetMesh& mesh() const { return mesh_; }

    LaplacianPair currentLaplacian() const;  // FEM L,M for u_ + ripple_

    // gestures (mirror RicciFlow)
    void perturb(double amplitude, unsigned seed);
    double step(double dt, double direction);   // +1 relax (diffuse), -1 sharpen
    void press(int vertex, double amount, double dt, double sigma);
    void strikeKick(int vertex, double amount, unsigned seed);
    void rippleStrike(int vertex, double amount);
    void rippleStep(double dt, double speed, double damp);
    bool rippleActive() const { return rippleEnergy_ > 1e-9; }
    void relaxToBase(double rate);
    void reset() { u_.setZero(); ripple_.setZero(); rippleVel_.setZero(); rippleEnergy_ = 0.0; }

    double curvatureError() const;   // max |graph-Laplacian(u)| — concentration peak
    double curvatureRms() const;     // RMS — the smooth Manual-servo variable
    double metricDeviation() const { return u_.cwiseAbs().maxCoeff(); }

    // viz: per-vertex heat (color) and displacement magnitude, up to maxN
    void fillViz(float* color, float* disp, int maxN) const;

private:
    Eigen::VectorXd graphLaplacian(const Eigen::VectorXd& f) const;  // neighbour avg - f
    void bfsBump(int vertex, double sigma, Eigen::VectorXd& out, int maxHops) const;

    TetMesh mesh_;
    Eigen::VectorXd u_;          // conformal factor (base = 0 = flat)
    Eigen::VectorXd ripple_, rippleVel_;
    double rippleEnergy_ = 0.0;
    double uClamp_ = 1.5;
};

} // namespace curv
