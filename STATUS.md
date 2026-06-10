# Project status

**Phase 0 — COMPLETE (2026-06-10). Holding at CHECKPOINT 0 for Julian's audition.**

## What exists

- `prototype/` Python rig: mesh generators (icosphere, intrinsically-flat
  lattice torus with arbitrary aspect, marching-cubes genus-2), intrinsic
  cotan Laplacian (edge lengths only), shift-invert `eigsh` mode solver,
  modal strike renderer (static + time-varying), metric-interpolation flow
  with overlap-based mode tracking, decorrelated-drift control generator.
- `prototype/oracles.py` — all four objective gates **pass**:
  - topology: genus 0/1/2 confirmed via Euler characteristic
  - icosphere: 8 degenerate groups follow l(l+1) within 5% (subdiv 4)
  - flat torus: 64 eigenvalues match the exact discrete lattice spectrum to
    max rel err ~5e-15 (plane waves verified as exact eigenvectors)
  - rendered FFT peaks within 1 cent of computed c·√λ_k (worst 0.000 cents)
- `renders/checkpoint0/` — 28 WAVs: {icosphere, torus 1:1, torus 1:1.618,
  genus-2} × {3 strike points} × {static, flow}, plus per-mesh
  decorrelated-drift A/B controls. See its MANIFEST.md.
- `assets/manifolds/` — OBJ exports of the preset meshes (display embeddings;
  metrics are intrinsic, see CLAUDE.md).

## Phase 0 scope decisions taken

- Flow is **metric interpolation** (conformal perturbation relaxing to the
  base metric in log-edge-length space), not full Chow–Luo — per the
  proposal's fast option for Open Decision 3. Real Ricci flow is Phase 2.
- The torus presets use the **flat lattice metric** with a donut display
  embedding, so the lattice-spectrum oracle is exact in the discrete setting.
- Mode tracking across flow frames uses mass-weighted eigenvector overlap
  (greedy assignment), keeping trajectories continuous through crossings.

## CHECKPOINT 0 — what Julian decides

1. **Claim 1 (genus taxonomy):** do sphere / torus / genus-2 sound like
   different *kinds* of object? (bell vs lattice vs dense shimmer)
2. **Claim 2 (coherence):** `*_flow.wav` vs `*_flow_decorrelated.wav` — does
   correlated drift read as one object changing? This is the load-bearing
   perceptual claim; kill/pivot/proceed.
3. Damping defaults (`T60=5s`, `tilt=0.7`, mallet 2200 Hz) — retune by ear
   here, it's the cheapest place.
4. The five open decisions in PROPOSAL.md §8 (platform target, mode count,
   MPE plumbing, license posture) before Phase 1 starts.

## Next (pending checkpoint verdict)

Phase 1 — JUCE 8 plugin skeleton with static geometry: CMake project, VST3 +
Standalone, Eigen/Spectra port of the Laplacian/eigensolver, full two-clock
architecture with flow disabled, pluginval + sanitizer gates.
