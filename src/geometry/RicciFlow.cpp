#include "RicciFlow.h"

#include <cmath>
#include <map>
#include <random>
#include <stdexcept>

namespace curv {

RicciFlow::RicciFlow(const Mesh& mesh, double uClamp)
    : mesh_(&mesh), uClamp_(uClamp)
{
    const int n = mesh.numVertices();

    std::map<std::pair<int, int>, int> edgeIndex;
    std::map<std::pair<int, int>, double> edgeLength;
    faceEdge_.resize(mesh.F.size());
    static constexpr int opp[3][2] = { { 1, 2 }, { 2, 0 }, { 0, 1 } };
    for (size_t fi = 0; fi < mesh.F.size(); ++fi)
        for (int j = 0; j < 3; ++j) {
            const int a = mesh.F[fi][opp[j][0]], b = mesh.F[fi][opp[j][1]];
            const auto key = std::pair { std::min(a, b), std::max(a, b) };
            auto [it, inserted] = edgeIndex.emplace(key, (int) edges_.size());
            if (inserted)
                edges_.push_back({ key.first, key.second });
            edgeLength[key] = mesh.faceLengths[fi][j];
            faceEdge_[fi][j] = it->second;
        }

    // initial radii: third of the min incident edge keeps inversive distances positive
    Eigen::VectorXd g0 = Eigen::VectorXd::Constant(n, 1e300);
    for (const auto& [key, l] : edgeLength) {
        g0[key.first] = std::min(g0[key.first], l / 3.0);
        g0[key.second] = std::min(g0[key.second], l / 3.0);
    }
    u0_ = g0.array().log();
    u_ = u0_;

    invDist_.resize((Eigen::Index) edges_.size());
    for (size_t e = 0; e < edges_.size(); ++e) {
        const double l = edgeLength.at({ edges_[e][0], edges_[e][1] });
        const double ga = g0[edges_[e][0]], gb = g0[edges_[e][1]];
        invDist_[(Eigen::Index) e] = (l * l - ga * ga - gb * gb) / (2.0 * ga * gb);
        if (invDist_[(Eigen::Index) e] <= 0.0)
            throw std::runtime_error("initial radii too large for inversive packing");
    }

    const int chi = mesh.eulerCharacteristic();
    kTarget_ = 2.0 * M_PI * chi / n;

    adjacency_.assign((size_t) n, {});
    for (const auto& e : edges_) {
        adjacency_[(size_t) e[0]].push_back(e[1]);
        adjacency_[(size_t) e[1]].push_back(e[0]);
    }

    ripple_ = Eigen::VectorXd::Zero(n);
    rippleVel_ = Eigen::VectorXd::Zero(n);
    morph_ = Eigen::VectorXd::Zero(n);
}

void RicciFlow::faceLengthsFor(const Eigen::VectorXd& u,
                               std::vector<std::array<double, 3>>& out) const
{
    const Eigen::VectorXd g = u.array().exp();
    out.resize(faceEdge_.size());
    for (size_t fi = 0; fi < faceEdge_.size(); ++fi)
        for (int j = 0; j < 3; ++j) {
            const auto& e = edges_[(size_t) faceEdge_[fi][j]];
            const double ga = g[e[0]], gb = g[e[1]];
            const double I = invDist_[faceEdge_[fi][j]];
            out[fi][j] = std::sqrt(ga * ga + gb * gb + 2.0 * I * ga * gb);
        }
}

Eigen::VectorXd RicciFlow::curvatures(const Eigen::VectorXd& u) const
{
    std::vector<std::array<double, 3>> l;
    faceLengthsFor(u, l);

    Eigen::VectorXd K = Eigen::VectorXd::Constant(mesh_->numVertices(), 2.0 * M_PI);
    for (size_t fi = 0; fi < l.size(); ++fi) {
        const double l0 = l[fi][0], l1 = l[fi][1], l2 = l[fi][2];
        const double a0 = std::acos(std::clamp((l1 * l1 + l2 * l2 - l0 * l0) / (2 * l1 * l2), -1.0, 1.0));
        const double a1 = std::acos(std::clamp((l2 * l2 + l0 * l0 - l1 * l1) / (2 * l2 * l0), -1.0, 1.0));
        const double a2 = std::acos(std::clamp((l0 * l0 + l1 * l1 - l2 * l2) / (2 * l0 * l1), -1.0, 1.0));
        K[mesh_->F[fi][0]] -= a0;
        K[mesh_->F[fi][1]] -= a1;
        K[mesh_->F[fi][2]] -= a2;
    }
    return K;
}

bool RicciFlow::isValid(const Eigen::VectorXd& u) const
{
    std::vector<std::array<double, 3>> l;
    faceLengthsFor(u, l);
    for (const auto& f : l) {
        const double s = 0.5 * (f[0] + f[1] + f[2]);
        if (s * (s - f[0]) * (s - f[1]) * (s - f[2]) <= 0.0)
            return false;
    }
    return true;
}

double RicciFlow::curvatureError() const
{
    return (curvatures(u_).array() - kTarget_).abs().maxCoeff();
}

double RicciFlow::curvatureRms() const
{
    const Eigen::VectorXd dev = curvatures(u_).array() - kTarget_;
    return std::sqrt(dev.squaredNorm() / (double) dev.size());
}

double RicciFlow::step(double dt, double direction)
{
    const Eigen::VectorXd K = curvatures(u_);
    const Eigen::VectorXd du = direction * (kTarget_ - K.array());
    const double errBefore = (K.array() - kTarget_).abs().maxCoeff();

    while (dt > 1e-7) {
        Eigen::VectorXd uNew = u_ + dt * du;
        uNew = u0_.array() + (uNew - u0_).array().min(uClamp_).max(-uClamp_);
        if (isValid(uNew)) {
            if (direction <= 0.0) {
                u_ = std::move(uNew);
                return dt;
            }
            const double errAfter = (curvatures(uNew).array() - kTarget_).abs().maxCoeff();
            if (errAfter <= errBefore + 1e-12) {
                u_ = std::move(uNew);
                return dt;
            }
        }
        dt *= 0.5;
    }
    return 0.0;
}

void RicciFlow::relaxToBase(double rate)
{
    double r = std::min(rate, 1.0);
    for (int tries = 0; tries < 6; ++tries) {
        Eigen::VectorXd uNew = u_ + r * (u0_ - u_);
        if (isValid(uNew)) {
            u_ = std::move(uNew);
            return;
        }
        r *= 0.5;
    }
}

void RicciFlow::strikeKick(int vertex, double amount, unsigned seed)
{
    const int n = mesh_->numVertices();

    // smooth random field (a few neighbour-averaging passes over white noise)
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd;
    Eigen::VectorXd r(n);
    for (int i = 0; i < n; ++i)
        r[i] = nd(rng);
    for (int pass = 0; pass < 4; ++pass) {
        Eigen::VectorXd avg(n);
        for (int i = 0; i < n; ++i) {
            double s = 0.0;
            for (int nb : adjacency_[(size_t) i])
                s += r[nb];
            avg[i] = adjacency_[(size_t) i].empty()
                         ? r[i] : s / (double) adjacency_[(size_t) i].size();
        }
        r = 0.5 * r + 0.5 * avg;
    }

    // window to a neighbourhood of the strike vertex (BFS distance, ~3-hop sigma)
    std::vector<int> dist((size_t) n, -1);
    std::vector<int> queue { vertex };
    dist[(size_t) vertex] = 0;
    for (size_t qi = 0; qi < queue.size(); ++qi)
        for (int nb : adjacency_[(size_t) queue[qi]])
            if (dist[(size_t) nb] < 0 && dist[(size_t) queue[qi]] < 7) {
                dist[(size_t) nb] = dist[(size_t) queue[qi]] + 1;
                queue.push_back(nb);
            }
    Eigen::VectorXd field = Eigen::VectorXd::Zero(n);
    for (int i = 0; i < n; ++i)
        if (dist[(size_t) i] >= 0) {
            const double d = dist[(size_t) i] / 3.0;
            field[i] = r[i] * std::exp(-d * d);
        }

    // mean-free + normalized, so the kick is scale-neutral (self-correcting)
    field.array() -= field.mean();
    const double peak = field.cwiseAbs().maxCoeff();
    if (peak < 1e-12)
        return;
    field *= amount / peak;

    double scale = 1.0;
    for (int tries = 0; tries < 8; ++tries) {
        Eigen::VectorXd uNew = u_ + scale * field;
        uNew = u0_.array() + (uNew - u0_).array().min(uClamp_).max(-uClamp_);
        if (isValid(uNew)) {
            u_ = std::move(uNew);
            return;
        }
        scale *= 0.5;
    }
}

void RicciFlow::press(int vertex, double amount, double dt, double sigma)
{
    if (vertex != pressVertex_ || sigma != pressSigma_) {
        // BFS graph distance from the pressed vertex; bump = exp(-(d/sigma)^2)
        const int n = mesh_->numVertices();
        std::vector<int> dist((size_t) n, -1);
        std::vector<int> queue { vertex };
        dist[(size_t) vertex] = 0;
        for (size_t qi = 0; qi < queue.size(); ++qi)
            for (int nb : adjacency_[(size_t) queue[qi]])
                if (dist[(size_t) nb] < 0) {
                    dist[(size_t) nb] = dist[(size_t) queue[qi]] + 1;
                    queue.push_back(nb);
                }
        pressProfile_.resize(n);
        for (int i = 0; i < n; ++i) {
            const double d = dist[(size_t) i] / std::max(sigma, 0.3);
            pressProfile_[i] = std::exp(-d * d);
        }
        // mean-free: flow conserves total log-scale, so a net-positive press
        // would leave a permanent uniform residual after relax. Subtracting
        // the mean makes the press scale-neutral (bump here, micro-shrink
        // everywhere else) and relax genuinely restores the base metric.
        pressProfile_.array() -= pressProfile_.mean();
        pressVertex_ = vertex;
        pressSigma_ = sigma;
    }

    // localized conformal bump: concentrates curvature at the strike point
    // (a free deformation gesture; persists per Memory, healed by flow/Reset)
    double scale = amount * dt;
    for (int tries = 0; tries < 8; ++tries) {
        Eigen::VectorXd uNew = u_ + scale * pressProfile_;
        uNew = u0_.array() + (uNew - u0_).array().min(uClamp_).max(-uClamp_);
        if (isValid(uNew)) {
            u_ = std::move(uNew);
            return;
        }
        scale *= 0.5;
    }
}

void RicciFlow::perturb(double amplitude, unsigned seed)
{
    const int n = mesh_->numVertices();
    std::mt19937 rng(seed);
    std::normal_distribution<double> dist;
    Eigen::VectorXd w(n);
    for (int i = 0; i < n; ++i)
        w[i] = dist(rng);

    std::vector<std::vector<int>> adj((size_t) n);
    for (const auto& e : edges_) {
        adj[(size_t) e[0]].push_back(e[1]);
        adj[(size_t) e[1]].push_back(e[0]);
    }
    for (int it = 0; it < 20; ++it) {
        Eigen::VectorXd avg(n);
        for (int i = 0; i < n; ++i) {
            double s = 0.0;
            for (int j : adj[(size_t) i])
                s += w[j];
            avg[i] = s / (double) adj[(size_t) i].size();
        }
        w = 0.5 * w + 0.5 * avg;
    }
    w.array() -= w.mean();
    w *= amplitude / std::max(w.array().abs().maxCoeff(), 1e-12);

    Eigen::VectorXd uNew = u_ + w;
    while (!isValid(uNew)) {
        w *= 0.7;
        uNew = u_ + w;
    }
    u_ = std::move(uNew);
}

void RicciFlow::writeFaceLengths(Mesh& mesh) const
{
    // the eigensolver/curvature see the base metric plus the transient ripple
    // and the perpetual morph wave
    Eigen::VectorXd eff = u_;
    if (rippleEnergy_ > 1e-12) eff += ripple_;
    if (morphAmp_ > 1e-9) eff += morph_;
    faceLengthsFor(eff, mesh.faceLengths);
}

void RicciFlow::morphAdvance(double dPhase, double amp)
{
    morphAmp_ = amp;
    if (morphTheta_.size() != u_.size()) { morph_.setZero(u_.size()); return; }
    morphPhase_ += dPhase;
    // travelling conformal wave along the phase field; mean-free (scale-neutral)
    morph_ = amp * (morphTheta_.array() - morphPhase_).cos();
    morph_.array() -= morph_.mean();
}

void RicciFlow::rippleStrike(int vertex, double amount, double sigma)
{
    // localized displacement bump (graph-distance falloff = sigma hops) that
    // the wave step then propagates. Tighter sigma -> sharper wavefront / more
    // high-frequency ripple content.
    const int n = mesh_->numVertices();
    const int maxHops = std::max(2, (int) std::ceil(3.0 * sigma));
    std::vector<int> dist((size_t) n, -1);
    std::vector<int> queue { vertex };
    dist[(size_t) vertex] = 0;
    for (size_t qi = 0; qi < queue.size(); ++qi)
        for (int nb : adjacency_[(size_t) queue[qi]])
            if (dist[(size_t) nb] < 0 && dist[(size_t) queue[qi]] < maxHops) {
                dist[(size_t) nb] = dist[(size_t) queue[qi]] + 1;
                queue.push_back(nb);
            }
    // mean-free bump: the graph Laplacian's constant mode has no restoring
    // force, so any DC component would persist forever (a permanent metric
    // offset). Subtract the bump's mean so the wave fully returns to rest.
    Eigen::VectorXd bump = Eigen::VectorXd::Zero(n);
    for (int i = 0; i < n; ++i)
        if (dist[(size_t) i] >= 0) {
            const double d = dist[(size_t) i] / std::max(sigma, 0.4);
            bump[i] = amount * std::exp(-d * d);
        }
    bump.array() -= bump.mean();
    ripple_ += bump;
    rippleEnergy_ = ripple_.squaredNorm() + rippleVel_.squaredNorm();
}

void RicciFlow::rippleStep(double dt, double speed, double damp)
{
    if (rippleEnergy_ <= 1e-12 && ripple_.squaredNorm() < 1e-18)
        return;
    const int n = mesh_->numVertices();
    // wave equation on the graph: a = speed^2 * (neighbour avg - r) - damp*v
    Eigen::VectorXd accel(n);
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int nb : adjacency_[(size_t) i])
            s += ripple_[nb];
        const double avg = adjacency_[(size_t) i].empty()
                               ? ripple_[i] : s / (double) adjacency_[(size_t) i].size();
        // weak spring toward zero (kSpring) gives even the DC mode a restoring
        // force so the wave always decays fully to rest
        accel[i] = speed * speed * (avg - ripple_[i]) - damp * rippleVel_[i]
                   - 0.5 * ripple_[i];
    }
    rippleVel_ += dt * accel;
    ripple_ += dt * rippleVel_;
    // keep the transient bounded (a degenerate u_+ripple_ just coasts now)
    ripple_ = ripple_.cwiseMax(-1.0).cwiseMin(1.0);
    rippleEnergy_ = ripple_.squaredNorm() + rippleVel_.squaredNorm();
}

} // namespace curv
