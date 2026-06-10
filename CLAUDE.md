# curvsynth — Claude Code project rules

Synthesis engine where the instrument is a Riemannian 2-manifold and the
performance is a geometric flow. Full design: `PROPOSAL.md`. Current phase
status: `STATUS.md`.

## Build & test commands

Phase 0 (Python audition rig):

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m prototype.oracles           # objective gates; exit 0 = pass
.venv/bin/python -m prototype.audition_matrix   # renders WAVs to renders/checkpoint0/
```

Phase 1+ (JUCE/CMake — not yet created):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build
tools/run_pluginval.sh                          # strictness 10, must pass
build/.../render_offline ...                    # headless WAV render for spectral oracles
```

## Invariants (all phases)

- **Oracles never go dark.** Any DSP change must keep the offline render path
  building (`prototype/render.py` now; `tools/render_offline.cpp` once C++
  exists) so FFT-vs-eigenvalue oracles always run. Run
  `python -m prototype.oracles` before declaring any geometry/DSP change done.
- **The metric is intrinsic.** Laplacians are built from edge lengths only
  (law of cosines + Heron), never vertex positions. Embeddings are for
  display and strike-point picking. The flat torus has a flat *metric* with a
  donut *embedding* — do not "fix" this.
- **Eigenvector conventions:** stiffness L symmetric PSD, mass M
  barycentric-lumped diagonal, generalized problem `L φ = λ M φ`,
  eigenvectors M-orthonormal, trivial λ₀=0 mode dropped.
- **Spectrum mapping:** `f_k = c·√λ_k`, `c = f_note/√λ_1` — the fundamental
  tracks the first nonzero eigenvalue; the whole spectrum transposes rigidly.

## Hard real-time rules (audio thread, Phase 1+)

Prohibited on the audio thread, no exceptions:
- allocation/deallocation (incl. anything that may resize, `std::string`,
  logging), locks/mutexes/condition variables, system calls, file or network
  I/O, blocking on the geometry thread.
- All FIFO slots, voice state, and coefficient arrays preallocated in
  `prepareToPlay`.
- Eigensolves never block audio: if the geometry thread falls behind, audio
  coasts on the last `SpectrumFrame` (audible as stasis, never a glitch).
- Resonators are coupled-form (rotation form), not direct-form biquads —
  stability under continuous frequency sweeps is the whole modulation story.
- Coefficient changes ramp linearly over one control interval (no zipper).

## Checkpoint protocol

Each phase ends at a human audition CHECKPOINT. At a checkpoint: **stop**,
summarize state in `STATUS.md`, make sure audition artifacts are rendered and
committed (`renders/checkpointN/` + `MANIFEST.md` describing what to listen
for), push, and wait for Julian's by-ear verdict before starting the next
phase. Do not begin Phase N+1 work speculatively.

## Phase gates (oracles)

- **Phase 0:** topology (Euler characteristic), icosphere l(l+1) degeneracy
  structure, flat-torus exact discrete lattice spectrum, rendered FFT peaks
  within 1 cent of computed `c·√λ_k`. → `prototype/oracles.py`
- **Phase 1:** pluginval strictness 10; C++ eigenvalues match Phase 0 Python
  within tolerance; FFT oracle ported; ASan+TSan clean; zero audio-thread
  allocations.
- **Phase 2:** forward flow: max |K_i − K̄| monotonically decreasing on all
  presets; eigenvalue trajectories continuous between scheduled re-solves;
  30-min soak with no NaN/denormal/divergence; reverse-flow clamps respected.
- **Phase 3 (surgery):** pinch detection fires before numerical degeneracy;
  child meshes pass Euler-characteristic checks; spectrum handoff continuous
  in energy.
