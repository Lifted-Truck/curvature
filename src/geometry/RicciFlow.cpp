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

void RicciFlow::press(int vertex, double amount, double dt, double sigma)
{
    if (vertex != pressVertex_ || sigma != pressSigma_) {
        // BFS graph distance from the pressed vertex; bump = exp(-(d/2.2)^2)
        const int n = mesh_->numVertices();
        std::vector<int> dist((size_t) n, -1);
        std::vector<std::vector<int>> adj((size_t) n);
        for (const auto& e : edges_) {
            adj[(size_t) e[0]].push_back(e[1]);
            adj[(size_t) e[1]].push_back(e[0]);
        }
        std::vector<int> queue { vertex };
        dist[(size_t) vertex] = 0;
        for (size_t qi = 0; qi < queue.size(); ++qi)
            for (int nb : adj[(size_t) queue[qi]])
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
    faceLengthsFor(u_, mesh.faceLengths);
}

} // namespace curv
