// Preset manifolds, ported 1:1 from prototype/meshes.py so the Phase 1
// oracle can compare eigenvalues against the committed Python dumps.
#pragma once

#include "Mesh.h"
#include "TetMesh.h"

namespace curv {

Mesh makeIcosphere(int subdiv = 3, double radius = 1.0);

// Intrinsically flat lattice torus: faceLengths come from the a x b lattice,
// the donut embedding is display-only. Do not "fix" this (CLAUDE.md).
Mesh makeFlatTorus(int nx, int ny, double a, double b);

Mesh loadObjFromString(const char* data, size_t size, const std::string& name);

enum class PresetId { Icosphere = 0, Torus11, TorusGolden, TorusString, Genus2,
                      Mandelbulb, Torus3D, Torus3DAniso, Torus3DOblique, Count };

inline const char* presetName(PresetId id)
{
    switch (id) {
        case PresetId::Icosphere:    return "Icosphere (bell)";
        case PresetId::Torus11:      return "Torus 1:1 (lattice)";
        case PresetId::TorusGolden:  return "Torus 1:1.618 (lattice)";
        case PresetId::TorusString:  return "Torus 8:1 (harmonic)";
        case PresetId::Genus2:       return "Genus 2 (shimmer)";
        case PresetId::Mandelbulb:   return "Mandelbulb (alien)";
        case PresetId::Torus3D:       return "3-Torus (4D)";
        case PresetId::Torus3DAniso:  return "3-Torus aniso (4D)";
        case PresetId::Torus3DOblique:return "3-Torus oblique (4D)";
        default:                      return "?";
    }
}

// true for presets loaded from an OBJ asset (Genus2, Mandelbulb)
inline bool presetNeedsObj(PresetId id)
{
    return id == PresetId::Genus2 || id == PresetId::Mandelbulb;
}

// true for 3-manifold (4D) presets — handled by TetManifold, not RicciFlow
inline bool presetIs4D(PresetId id)
{
    return id == PresetId::Torus3D || id == PresetId::Torus3DAniso
           || id == PresetId::Torus3DOblique;
}

// build the tet mesh for a 4D preset
inline TetMesh makeTetPreset(PresetId id)
{
    if (id == PresetId::Torus3DAniso)
        return makeFlatTorus3(12, 12, 12, 1.0, 1.32, 1.71);
    if (id == PresetId::Torus3DOblique) {
        // equal-length basis vectors at oblique (60 deg) angles — a
        // rhombohedral/"isosceles" lattice; the sheared reciprocal lattice
        // gives a different shell structure than the cubic one (columns)
        const double s = 0.5, h = 0.8660254, t = 0.8164966, u = 0.2886751;
        const double basis[3][3] = { { 1.0, s,   s   },
                                     { 0.0, h,   u   },
                                     { 0.0, 0.0, t   } };
        return makeLatticeTorus3(12, 12, 12, basis);
    }
    return makeFlatTorus3(12, 12, 12, 1.0, 1.0, 1.0);
}

// OBJ-based presets take their mesh asset (plugin uses BinaryData, tools read
// the file); analytic presets ignore obj.
Mesh makePreset(PresetId id, const char* obj, size_t objSize);

} // namespace curv
