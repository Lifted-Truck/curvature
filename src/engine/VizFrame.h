// Geometry -> editor visualization snapshot: per-vertex curvature deviation
// (K - Kbar) and conformal scale (u - u0), plus the current spectrum ratios.
// Fixed-size POD for the triple buffer; the editor (message thread) is the
// sole consumer, the geometry thread the sole producer.
#pragma once

#include <cstdint>

#include "SpectrumFrame.h"
#include "TripleBuffer.h"

namespace curv {

constexpr int kMaxVizVerts = 4096;

struct VizFrame {
    int numVerts = 0;
    int numModes = 0;
    int presetId = -1;
    int strikeVertex = 0;
    float curvatureErr = 0.0f;
    uint32_t frameId = 0;
    float kDev[kMaxVizVerts] = {};   // curvature deviation per vertex
    float uDev[kMaxVizVerts] = {};   // conformal log-scale per vertex
    float ratio[kMaxModes] = {};     // current f_k / f_1
};

using VizBus = TripleBuffer<VizFrame>;

} // namespace curv
