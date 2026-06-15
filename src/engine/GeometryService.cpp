#include "GeometryService.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "VizFrame.h"

namespace curv {

void GeometryService::loadPreset(PresetId id, const char* obj, size_t objSize)
{
    is4D_ = presetIs4D(id);
    if (is4D_) {
        tet_ = std::make_unique<TetManifold>(makeTetPreset(id));
        flow_.reset();
        mesh_ = Mesh();
    } else {
        mesh_ = makePreset(id, obj, objSize);
        flow_ = std::make_unique<RicciFlow>(mesh_);
        tet_.reset();
    }
    modes_ = solveModes(currentLaplacian(), kMaxModes);
    lambda_ = modes_.lambda;
    blendRemaining_ = 0;
    lambdaPreResolve_.resize(0);

    // morph phase field = a rotatable blend of the base first two
    // eigenfunctions. phi_1's gradient is a natural travel axis (around a
    // cycle on the torus, pole-to-pole on the sphere); Morph Angle rotates it
    // in the phi_1/phi_2 plane for different sweep directions. Captured once
    // from the base geometry so the axis is stable.
    morphPhi_.resize(modes_.phi.rows(), 2);
    morphPhi_.col(0) = modes_.phi.col(0);
    morphPhi_.col(1) = modes_.phi.col(1);
    setMorphAngle(0.0);
}

void GeometryService::setMorphAngle(double angle)
{
    if (morphPhi_.cols() < 2)
        return;
    Eigen::VectorXd theta = std::cos(angle) * morphPhi_.col(0)
                            + std::sin(angle) * morphPhi_.col(1);
    const double peak = theta.cwiseAbs().maxCoeff();
    if (peak > 1e-12)
        theta *= M_PI / peak;  // normalize to [-pi, pi]
    if (is4D_) tet_->setMorphField(theta);
    else flow_->setMorphField(theta);
}

LaplacianPair GeometryService::currentLaplacian() const
{
    return is4D_ ? tet_->currentLaplacian() : buildCotanLaplacian(mesh_);
}

int GeometryService::numVertices() const
{
    return is4D_ ? tet_->numVertices() : mesh_.numVertices();
}

int GeometryService::strikeVertex(float strikeParam) const
{
    const int n = numVertices();
    return std::clamp(static_cast<int>(std::lround(strikeParam * (n - 1))), 0, n - 1);
}

void GeometryService::fillFrame(SpectrumFrame& frame, int numModes, float strikeParam) const
{
    const int k = std::clamp(numModes, 1, std::min(kMaxModes, (int) lambda_.size()));
    const int vtx = strikeVertex(strikeParam);
    const double lam1 = std::max(lambda_[0], 1e-12);

    frame.numModes = k;
    frame.frameId = nextFrameId_++;

    double peak = 0.0;
    for (int m = 0; m < k; ++m)
        peak = std::max(peak, std::abs(modes_.phi(vtx, m)));
    const double couplingScale = peak > 0.0 ? 1.0 / peak : 0.0;

    for (int m = 0; m < k; ++m) {
        float r = static_cast<float>(std::sqrt(std::max(lambda_[m], 0.0) / lam1));
        float c = static_cast<float>(modes_.phi(vtx, m) * couplingScale);
        // a degenerate solve must never poison the audio thread: non-finite
        // entries fall back to the previous mode's ratio / silence
        if (!std::isfinite(r))
            r = m > 0 ? frame.ratio[m - 1] : 1.0f;
        if (!std::isfinite(c))
            c = 0.0f;
        frame.ratio[m] = r;
        frame.coupling[m] = c;
    }
}

// ---------------------------------------------------------------- flow

void GeometryService::flowKick(double amplitude, unsigned seed)
{
    if (is4D_) tet_->perturb(amplitude, seed);
    else { flow_->perturb(amplitude, seed); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

double GeometryService::flowStep(double dt, double direction)
{
    const double taken = is4D_ ? tet_->step(dt, direction) : flow_->step(dt, direction);
    if (taken > 0.0) {
        if (!is4D_) flow_->writeFaceLengths(mesh_);
        rayleighUpdate();
    }
    return taken;
}

void GeometryService::flowPress(float strikeParam, double amount, double dt, double sigma)
{
    const int v = strikeVertex(strikeParam);
    if (is4D_) tet_->press(v, amount, dt, sigma);
    else { flow_->press(v, amount, dt, sigma); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

void GeometryService::fillVizFrame(VizFrame& frame, int numModes, float strikeParam,
                                   int presetId) const
{
    const int nv = std::min(numVertices(), kMaxVizVerts);
    frame.numVerts = nv;
    frame.presetId = presetId;
    frame.strikeVertex = strikeVertex(strikeParam);
    frame.frameId = nextFrameId_++;

    if (is4D_) {
        tet_->fillViz(frame.kDev, frame.uDev, nv);
        frame.curvatureErr = static_cast<float>(tet_->curvatureError());
    } else {
        const Eigen::VectorXd kDev = flow_->curvatureDeviation();
        // include the transient ripple + perpetual morph so both are visible
        const Eigen::VectorXd uDev = flow_->logRadii() - flow_->logRadiiBase()
                                     + flow_->rippleField() + flow_->morphField();
        frame.curvatureErr = static_cast<float>(kDev.cwiseAbs().maxCoeff());
        for (int i = 0; i < nv; ++i) {
            frame.kDev[i] = static_cast<float>(kDev[i]);
            frame.uDev[i] = static_cast<float>(uDev[i]);
        }
    }

    const int k = std::clamp(numModes, 1, std::min(kMaxModes, (int) lambda_.size()));
    frame.numModes = k;
    const double lam1 = std::max(lambda_[0], 1e-12);
    for (int m = 0; m < k; ++m)
        frame.ratio[m] = static_cast<float>(std::sqrt(std::max(lambda_[m], 0.0) / lam1));
}

void GeometryService::strikeKick(float strikeParam, double amount, unsigned seed)
{
    const int v = strikeVertex(strikeParam);
    if (is4D_) tet_->strikeKick(v, amount, seed);
    else { flow_->strikeKick(v, amount, seed); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

void GeometryService::rippleStrike(float strikeParam, double amount, double sigma)
{
    const int v = strikeVertex(strikeParam);
    if (is4D_) tet_->rippleStrike(v, amount, sigma);
    else { flow_->rippleStrike(v, amount, sigma); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

void GeometryService::rippleStep(double dt, double speed, double damp)
{
    if (is4D_) tet_->rippleStep(dt, speed, damp);
    else { flow_->rippleStep(dt, speed, damp); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

void GeometryService::morphStep(double dPhase, double amp)
{
    if (is4D_) tet_->morphAdvance(dPhase, amp);
    else { flow_->morphAdvance(dPhase, amp); flow_->writeFaceLengths(mesh_); }
    rayleighUpdate();
}

void GeometryService::flowElastic(double rate)
{
    if (is4D_) { tet_->relaxToBase(rate); rayleighUpdate(); return; }
    flow_->relaxToBase(rate);
    flow_->writeFaceLengths(mesh_);
    rayleighUpdate();
}

double GeometryService::metricDeviation() const
{
    return is4D_ ? tet_->metricDeviation()
                 : (flow_->logRadii() - flow_->logRadiiBase()).cwiseAbs().maxCoeff();
}

void GeometryService::flowReset()
{
    if (is4D_) tet_->reset();
    else { flow_->reset(); flow_->writeFaceLengths(mesh_); }
    resolve();
    blendRemaining_ = 0;     // reset is a discontinuity by request: no blend
    lambda_ = modes_.lambda;
}

void GeometryService::rayleighUpdate()
{
    // Rayleigh quotients of the (stale) eigenbasis with the current
    // operator: second-order accurate in the basis drift, never NaN, and
    // costs k sparse quadratic forms — the real-time escape hatch
    // (PROPOSAL.md section 4.3). If the (possibly rippled) metric degenerates
    // a triangle, buildCotanLaplacian throws — coast on the last values
    // rather than letting it reset the engine.
    LaplacianPair lap;
    try {
        lap = currentLaplacian();
    } catch (...) {
        return;
    }
    for (int m = 0; m < (int) lambda_.size(); ++m) {
        const auto& phi = modes_.phi.col(m);
        const double num = phi.dot(lap.L * phi);
        const double den = phi.dot(lap.massDiag.asDiagonal() * phi);
        const double lam = num / std::max(den, 1e-300);
        if (std::isfinite(lam))
            lambda_[m] = std::max(lam, 0.0);
    }

    // post-resolve reconciliation: walk the published values from the old
    // fast-path trajectory into the fresh basis over kBlendFrames frames so
    // a re-solve never lands as an audible step
    if (blendRemaining_ > 0) {
        const double w = (double) blendRemaining_ / (kBlendFrames + 1);
        lambda_ = w * lambdaPreResolve_ + (1.0 - w) * lambda_;
        --blendRemaining_;
    }
}

void GeometryService::resolve()
{
    // At extreme curvature concentration the eigensolver can fail to converge
    // (or the metric can degenerate). That must NOT reset the instrument — per
    // the engine's rule, coast on the last good spectrum. A failed or garbage
    // solve is swallowed here and we keep the current basis; the object holds
    // at the limit instead of crashing and reloading.
    LaplacianPair lap;
    ModeSet fresh;
    try {
        lap = currentLaplacian();
        fresh = solveModes(lap, kMaxModes);
    } catch (...) {
        return;  // solve failed: coast on the previous modes
    }
    if (!fresh.lambda.allFinite() || !fresh.phi.allFinite())
        return;  // garbage solve: keep the last good basis

    // greedy matching against the previous basis so mode identities (and
    // therefore frame ratios) stay continuous through the re-solve. Score =
    // eigenvector overlap weighted by eigenvalue proximity to the fast-path
    // estimate: overlap alone goes ambiguous for high modes once the basis
    // has drifted, and proximity alone cannot track true crossings — the
    // product disambiguates both.
    const int k = (int) fresh.lambda.size();
    Eigen::MatrixXd overlap =
        (modes_.phi.transpose() * lap.massDiag.asDiagonal() * fresh.phi).cwiseAbs();
    for (int slot = 0; slot < k; ++slot)
        for (int cur = 0; cur < k; ++cur) {
            const double logDist = std::abs(std::log(std::max(fresh.lambda[cur], 1e-300) /
                                                     std::max(lambda_[slot], 1e-300)));
            overlap(slot, cur) *= std::exp(-8.0 * logDist);
        }

    std::vector<int> order(static_cast<size_t>(k) * (size_t) k);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return overlap(a / k, a % k) > overlap(b / k, b % k);
    });

    std::vector<int> assign((size_t) k, -1);
    std::vector<bool> taken((size_t) k, false);
    int filled = 0;
    for (int idx : order) {
        const int slot = idx / k, cur = idx % k;
        if (assign[(size_t) slot] == -1 && !taken[(size_t) cur]) {
            assign[(size_t) slot] = cur;
            taken[(size_t) cur] = true;
            if (++filled == k)
                break;
        }
    }

    ModeSet matched;
    matched.lambda.resize(k);
    matched.phi.resize(fresh.phi.rows(), k);
    for (int slot = 0; slot < k; ++slot) {
        matched.lambda[slot] = fresh.lambda[assign[(size_t) slot]];
        matched.phi.col(slot) = fresh.phi.col(assign[(size_t) slot]);
    }

    lambdaPreResolve_ = lambda_;  // published trajectory up to this instant
    blendRemaining_ = kBlendFrames;
    modes_ = std::move(matched);
    lambda_ = modes_.lambda;
    if (lambdaPreResolve_.size() == lambda_.size()) {
        const double w = (double) blendRemaining_ / (kBlendFrames + 1);
        lambda_ = w * lambdaPreResolve_ + (1.0 - w) * lambda_;
        --blendRemaining_;
    } else {
        blendRemaining_ = 0;  // first solve / mode-count change: nothing to blend
    }
}

} // namespace curv
