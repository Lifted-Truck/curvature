// Non-realtime geometry work: build preset mesh, solve modes, evaluate
// strike coupling, fill SpectrumFrames. Threading is the caller's concern
// (the plugin wraps this in a juce::Thread; tools call it synchronously).
#pragma once

#include <cstdint>

#include "../geometry/EigenSolver.h"
#include "../geometry/Presets.h"
#include "SpectrumFrame.h"

namespace curv {

class GeometryService {
public:
    void loadPreset(PresetId id, const char* genus2Obj, size_t genus2ObjSize);

    // strikeParam in [0,1] maps to a vertex index
    void fillFrame(SpectrumFrame& frame, int numModes, float strikeParam) const;

    const Mesh& mesh() const { return mesh_; }
    const ModeSet& modes() const { return modes_; }
    int strikeVertex(float strikeParam) const;

private:
    Mesh mesh_;
    ModeSet modes_;
    mutable uint32_t nextFrameId_ = 1;
};

} // namespace curv
