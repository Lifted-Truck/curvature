#include "TetManifold.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace curv {

TetManifold::TetManifold(TetMesh mesh) : mesh_(std::move(mesh))
{
    const int n = mesh_.numVertices();
    u_ = Eigen::VectorXd::Zero(n);
    ripple_ = Eigen::VectorXd::Zero(n);
    rippleVel_ = Eigen::VectorXd::Zero(n);
    morph_ = Eigen::VectorXd::Zero(n);
}

Eigen::VectorXd TetManifold::graphLaplacian(const Eigen::VectorXd& f) const
{
    const int n = mesh_.numVertices();
    Eigen::VectorXd out(n);
    for (int i = 0; i < n; ++i) {
        const auto& nb = mesh_.adjacency[(size_t) i];
        double s = 0.0;
        for (int j : nb)
            s += f[j];
        out[i] = nb.empty() ? 0.0 : s / (double) nb.size() - f[i];
    }
    return out;
}

LaplacianPair TetManifold::currentLaplacian() const
{
    Eigen::VectorXd eff = u_;
    if (rippleEnergy_ > 1e-12) eff += ripple_;
    if (morphAmp_ > 1e-9) eff += morph_;
    return buildTetLaplacian(mesh_, eff);
}

void TetManifold::morphAdvance(double dPhase, double amp)
{
    morphAmp_ = amp;
    if (morphTheta_.size() != u_.size()) { morph_.setZero(u_.size()); return; }
    morphPhase_ += dPhase;
    morph_ = amp * (morphTheta_.array() - morphPhase_).cos();
    morph_.array() -= morph_.mean();
}

void TetManifold::bfsBump(int vertex, double sigma, Eigen::VectorXd& out, int maxHops) const
{
    const int n = mesh_.numVertices();
    std::vector<int> dist((size_t) n, -1);
    std::vector<int> queue { vertex };
    dist[(size_t) vertex] = 0;
    for (size_t qi = 0; qi < queue.size(); ++qi)
        for (int nb : mesh_.adjacency[(size_t) queue[qi]])
            if (dist[(size_t) nb] < 0 && dist[(size_t) queue[qi]] < maxHops) {
                dist[(size_t) nb] = dist[(size_t) queue[qi]] + 1;
                queue.push_back(nb);
            }
    out = Eigen::VectorXd::Zero(n);
    for (int i = 0; i < n; ++i)
        if (dist[(size_t) i] >= 0) {
            const double d = dist[(size_t) i] / std::max(sigma, 0.3);
            out[i] = std::exp(-d * d);
        }
}

double TetManifold::step(double dt, double direction)
{
    // conformal flow: relax (direction>0) diffuses u toward uniform via the
    // graph heat equation (the round 3-torus); sharpen (<0) anti-diffuses to
    // concentrate it. Mean-free + clamped so total scale doesn't drift.
    const Eigen::VectorXd lap = graphLaplacian(u_);
    Eigen::VectorXd uNew = u_ + (direction * dt) * lap;
    uNew.array() -= uNew.mean();
    u_ = uNew.cwiseMax(-uClamp_).cwiseMin(uClamp_);
    return dt;
}

void TetManifold::press(int vertex, double amount, double dt, double sigma)
{
    Eigen::VectorXd bump;
    bfsBump(vertex, sigma, bump, 12);
    bump.array() -= bump.mean();           // scale-neutral
    u_ = (u_ + (amount * dt) * bump).cwiseMax(-uClamp_).cwiseMin(uClamp_);
}

void TetManifold::strikeKick(int vertex, double amount, unsigned seed)
{
    const int n = mesh_.numVertices();
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd;
    Eigen::VectorXd r(n);
    for (int i = 0; i < n; ++i)
        r[i] = nd(rng);
    for (int pass = 0; pass < 4; ++pass)
        r += 0.5 * graphLaplacian(r);      // smooth the noise

    Eigen::VectorXd win;
    bfsBump(vertex, 3.0, win, 12);
    Eigen::VectorXd field = r.cwiseProduct(win);
    field.array() -= field.mean();
    const double peak = field.cwiseAbs().maxCoeff();
    if (peak < 1e-12)
        return;
    field *= amount / peak;
    u_ = (u_ + field).cwiseMax(-uClamp_).cwiseMin(uClamp_);
}

void TetManifold::rippleStrike(int vertex, double amount, double sigma)
{
    Eigen::VectorXd bump;
    bfsBump(vertex, sigma, bump, std::max(2, (int) std::ceil(3.0 * sigma)));
    bump.array() -= bump.mean();           // no DC (graph Laplacian has no restoring DC)
    ripple_ += amount * bump;
    rippleEnergy_ = ripple_.squaredNorm() + rippleVel_.squaredNorm();
}

void TetManifold::rippleStep(double dt, double speed, double damp)
{
    if (rippleEnergy_ <= 1e-12 && ripple_.squaredNorm() < 1e-18)
        return;
    const Eigen::VectorXd accel = (speed * speed) * graphLaplacian(ripple_)
                                  - damp * rippleVel_ - 0.5 * ripple_;
    rippleVel_ += dt * accel;
    ripple_ += dt * rippleVel_;
    ripple_ = ripple_.cwiseMax(-1.0).cwiseMin(1.0);
    rippleEnergy_ = ripple_.squaredNorm() + rippleVel_.squaredNorm();
}

void TetManifold::relaxToBase(double rate)
{
    u_ += std::min(rate, 1.0) * (Eigen::VectorXd::Zero(u_.size()) - u_);
}

void TetManifold::perturb(double amplitude, unsigned seed)
{
    const int n = mesh_.numVertices();
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd;
    Eigen::VectorXd w(n);
    for (int i = 0; i < n; ++i)
        w[i] = nd(rng);
    for (int pass = 0; pass < 20; ++pass)
        w += 0.5 * graphLaplacian(w);
    w.array() -= w.mean();
    w *= amplitude / std::max(w.cwiseAbs().maxCoeff(), 1e-12);
    u_ = (u_ + w).cwiseMax(-uClamp_).cwiseMin(uClamp_);
}

double TetManifold::curvatureError() const
{
    return graphLaplacian(u_ + ripple_).cwiseAbs().maxCoeff();
}

double TetManifold::curvatureRms() const
{
    const Eigen::VectorXd c = graphLaplacian(u_);
    return std::sqrt(c.squaredNorm() / (double) c.size());
}

void TetManifold::fillViz(float* color, float* disp, int maxN) const
{
    Eigen::VectorXd eff = u_ + ripple_;
    if (morphAmp_ > 1e-9) eff += morph_;
    const Eigen::VectorXd c = graphLaplacian(eff);          // concentration -> heat
    const Eigen::VectorXd& d = eff;                         // displacement
    const int n = std::min(numVertices(), maxN);
    for (int i = 0; i < n; ++i) {
        color[i] = static_cast<float>(c[i]);
        disp[i] = static_cast<float>(d[i]);
    }
}

} // namespace curv
