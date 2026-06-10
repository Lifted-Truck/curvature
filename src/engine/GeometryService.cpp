#include "GeometryService.h"

#include <algorithm>
#include <cmath>

namespace curv {

void GeometryService::loadPreset(PresetId id, const char* genus2Obj, size_t genus2ObjSize)
{
    mesh_ = makePreset(id, genus2Obj, genus2ObjSize);
    modes_ = solveModes(buildCotanLaplacian(mesh_), kMaxModes);
}

int GeometryService::strikeVertex(float strikeParam) const
{
    const int n = mesh_.numVertices();
    return std::clamp(static_cast<int>(std::lround(strikeParam * (n - 1))), 0, n - 1);
}

void GeometryService::fillFrame(SpectrumFrame& frame, int numModes, float strikeParam) const
{
    const int k = std::clamp(numModes, 1, std::min(kMaxModes, (int) modes_.lambda.size()));
    const int vtx = strikeVertex(strikeParam);
    const double lam1 = modes_.lambda[0];

    frame.numModes = k;
    frame.frameId = nextFrameId_++;

    double peak = 0.0;
    for (int m = 0; m < k; ++m)
        peak = std::max(peak, std::abs(modes_.phi(vtx, m)));
    const double couplingScale = peak > 0.0 ? 1.0 / peak : 0.0;

    for (int m = 0; m < k; ++m) {
        frame.ratio[m] = static_cast<float>(std::sqrt(modes_.lambda[m] / lam1));
        frame.coupling[m] = static_cast<float>(modes_.phi(vtx, m) * couplingScale);
    }
}

} // namespace curv
