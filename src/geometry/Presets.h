// Preset manifolds, ported 1:1 from prototype/meshes.py so the Phase 1
// oracle can compare eigenvalues against the committed Python dumps.
#pragma once

#include "Mesh.h"

namespace curv {

Mesh makeIcosphere(int subdiv = 3, double radius = 1.0);

// Intrinsically flat lattice torus: faceLengths come from the a x b lattice,
// the donut embedding is display-only. Do not "fix" this (CLAUDE.md).
Mesh makeFlatTorus(int nx, int ny, double a, double b);

Mesh loadObjFromString(const char* data, size_t size, const std::string& name);

enum class PresetId { Icosphere = 0, Torus11, TorusGolden, TorusString, Genus2,
                      Mandelbulb, Count };

inline const char* presetName(PresetId id)
{
    switch (id) {
        case PresetId::Icosphere:   return "Icosphere (bell)";
        case PresetId::Torus11:     return "Torus 1:1 (lattice)";
        case PresetId::TorusGolden: return "Torus 1:1.618 (lattice)";
        case PresetId::TorusString: return "Torus 8:1 (harmonic)";
        case PresetId::Genus2:      return "Genus 2 (shimmer)";
        case PresetId::Mandelbulb:  return "Mandelbulb (alien)";
        default:                    return "?";
    }
}

// true for presets loaded from an OBJ asset (Genus2, Mandelbulb)
inline bool presetNeedsObj(PresetId id)
{
    return id == PresetId::Genus2 || id == PresetId::Mandelbulb;
}

// OBJ-based presets take their mesh asset (plugin uses BinaryData, tools read
// the file); analytic presets ignore obj.
Mesh makePreset(PresetId id, const char* obj, size_t objSize);

} // namespace curv
