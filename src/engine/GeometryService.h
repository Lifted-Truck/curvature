// Non-realtime geometry work: build preset mesh, solve modes, evaluate
// strike coupling, fill SpectrumFrames — and, in Phase 2, advance the Ricci
// flow with perturbative eigenvalue updates between scheduled full
// re-solves. Threading is the caller's concern (the plugin wraps this in a
// juce::Thread; tools call it synchronously).
#pragma once

#include <cstdint>
#include <memory>

#include "../geometry/EigenSolver.h"
#include "../geometry/Presets.h"
#include "../geometry/RicciFlow.h"
#include "SpectrumFrame.h"

namespace curv {

class GeometryService {
public:
    void loadPreset(PresetId id, const char* genus2Obj, size_t genus2ObjSize);

    // strikeParam in [0,1] maps to a vertex index
    void fillFrame(SpectrumFrame& frame, int numModes, float strikeParam) const;

    // ---- Phase 2 flow API ----------------------------------------------
    // smooth conformal kick (the displacement RELAX relaxes from)
    void flowKick(double amplitude, unsigned seed);
    // one flow step (+1 RELAX, -1 SHARPEN) + fast-path eigenvalue update;
    // returns dt actually taken
    double flowStep(double dt, double direction);
    void flowReset();
    // scheduled full eigensolve, mode-matched to the previous basis so
    // frame-to-frame trajectories stay continuous through the re-solve
    void resolve();
    double curvatureError() const { return flow_->curvatureError(); }

    const Mesh& mesh() const { return mesh_; }
    const Eigen::VectorXd& lambda() const { return lambda_; }
    int strikeVertex(float strikeParam) const;

private:
    void rayleighUpdate();  // lambda_k <- (phi_k' L phi_k)/(phi_k' M phi_k)

    static constexpr int kBlendFrames = 8;  // post-resolve reconciliation span

    Mesh mesh_;
    ModeSet modes_;             // basis from the last full solve
    Eigen::VectorXd lambda_;    // current published eigenvalue estimates
    Eigen::VectorXd lambdaPreResolve_;
    int blendRemaining_ = 0;
    std::unique_ptr<RicciFlow> flow_;
    mutable uint32_t nextFrameId_ = 1;
};

} // namespace curv
