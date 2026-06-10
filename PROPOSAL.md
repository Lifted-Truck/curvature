# Curvature Synthesis — Build Proposal for Claude Code

**Deliverable:** VST3 instrument plugin, hosted in Ableton Live 12, built with JUCE 8 + CMake.
**Working title:** `curvsynth` (plugin display name TBD).
**Build methodology:** Phased autonomous runs with manual audition checkpoints, following the wtfoundry pattern: objective spectral oracles gate each phase; human ears gate aesthetic direction.

---

## 1. Thesis

A synthesis engine where the instrument is a Riemannian 2-manifold and the performance is a geometric flow:

- **Spectrum from geometry.** Partial frequencies are eigenvalues of the Laplace–Beltrami operator on a triangle mesh: `f_k = c·√λ_k`. Excitation coupling comes from eigenfunction values at the strike point: mode k rings with amplitude ∝ `φ_k(p)`.
- **Modulation from Ricci flow.** The metric evolves under discrete Ricci flow; eigenvalues drift smoothly and in a correlated way because all partials are spectra of one evolving object. Uniformization gives every patch an attractor timbre determined by topology alone.
- **Polyphony from surgery (stretch goal).** Neck-pinch singularities split the manifold; one voice becomes two.

**Rendering layer is a modal resonator bank** — deliberately conventional, so all novelty (and all risk) is isolated in the control manifold.

### Falsifiable claims the prototype tests

1. **Genus taxonomy.** Genus 0 flows toward near-harmonic bell spectra (`f ∝ l + ½`), genus 1 toward lattice spectra (`√(m²/a² + n²/b²)`), genus ≥ 2 toward dense GOE-statistics shimmer. If the rendered audio doesn't audibly differentiate by genus, the taxonomy claim dies.
2. **Coherence from covariance.** Correlated partial drift under flow should sound like *one object changing*, not N independent partials sliding. A/B against an additive bank with matched but decorrelated drift. This is the perceptual load-bearing claim.
3. **Flow as musical gesture.** Forward flow (curvature smoothing → spectral regularization) and reversed/perturbed flow (curvature sharpening → spectral spreading) should function as expressive envelope material, not just slow detuning.

If claims 1–2 fail at Phase 0, the project pivots or stops cheaply.

---

## 2. References

### Mathematical foundations
- Kac, M. (1966). "Can One Hear the Shape of a Drum?" *Amer. Math. Monthly* 73. — The inverse framing this engine runs forward.
- Reuter, M., Wolter, F.-E., Peinecke, N. (2006). "Laplace–Beltrami spectra as 'Shape-DNA' of surfaces and solids." *Computer-Aided Design* 38. — Establishes LB eigenvalues as practical, discretization-robust shape signatures; our spectrum source.
- Hamilton, R. (1982). "Three-manifolds with positive Ricci curvature." *J. Diff. Geom.* — Ricci flow origin.
- Chow, B., Luo, F. (2003). "Combinatorial Ricci flows on surfaces." *J. Diff. Geom.* 63. — The discrete (circle-packing) Ricci flow we implement; proven convergence to constant-curvature metrics.
- Jin, M., Kim, J., Luo, F., Gu, X. (2008). "Discrete Surface Ricci Flow." *IEEE TVCG* 14(5). — Practical algorithms, Newton-method acceleration.
- Springborn, B., Schröder, P., Pinkall, U. (2008). "Conformal Equivalence of Triangle Meshes." *SIGGRAPH*. — Alternative conformal-factor formulation (CETM); candidate if circle packing proves fiddly.
- Crane, K. *Discrete Differential Geometry: An Applied Introduction* (CMU course notes, freely available). — Cotan Laplacian construction, mesh data structures; primary implementation reference.
- Standard first-order eigenvalue perturbation theory: `dλ_k = φ_kᵀ·dL·φ_k` for symmetric L. — The real-time escape hatch (§4.3).
- Bohigas, O., Giannoni, M.-J., Schmit, C. (1984). "Characterization of chaotic quantum spectra." *PRL* 52. — GOE eigenvalue statistics on hyperbolic-type domains; grounds the genus ≥ 2 "structured shimmer" prediction.

### Nearest synthesis relatives (for honest differentiation)
- Adrien, J.-M. (1991). Modal synthesis; IRCAM **Modalys**. — Modal banks driven by *physical* object models. We differ in (a) non-realizable abstract manifolds, (b) PDE flow as modulation, (c) topology as patch taxonomy.
- Mathews, M., et al. — Scanned synthesis. Closest in spirit (a slowly evolving dynamical system scanned at audio rate) but evolves a *wavetable*, not an operator spectrum; no geometric attractor structure.
- Hiller/Ruiz, Karplus–Strong, banded waveguides (Essl/Cook) — physical-modeling lineage; cite to position, not to borrow.

### Libraries
- **JUCE 8.x** (GPL/commercial) — plugin framework, `juce::dsp` for the resonator bank.
- **Eigen 3.4** (header-only) — sparse matrices, dense small solves.
- **Spectra** (header-only, sits on Eigen) — Lanczos/shift-invert sparse symmetric eigensolver for the lowest 64–128 modes.
- **libigl** (header-only, optional) — reference cotan-Laplacian and mass-matrix construction (`igl::cotmatrix`, `igl::massmatrix`); we may hand-roll (~80 LOC) to keep deps light, but use libigl as the correctness oracle in tests.
- **pluginval** (Tracktion) — VST3 validation at strictness 10; CI gate.
- Phase 0 only: **Python** — `numpy`, `scipy.sparse.linalg.eigsh`, `libigl` python bindings or `robust_laplacian`, `soundfile`.

---

## 3. Sound model

### 3.1 Spectrum mapping
- Generalized eigenproblem `L φ = λ M φ` (cotan stiffness L, lumped mass M). Solve lowest K modes, K = 96 default (configurable 32–128).
- `f_k = c·√λ_k`, where `c` is set by MIDI note: the **fundamental tracks the first nonzero eigenvalue**, i.e. `c = f_note / √λ_1`. The whole spectrum transposes rigidly; the manifold defines interval structure, the keyboard defines register.
- Drop the trivial λ₀ = 0 (constant) mode.

### 3.2 Excitation
- **Strike:** energy injected per mode ∝ `φ_k(p)` at strike point p (a vertex or barycentric point), shaped by a "mallet" lowpass over mode index/frequency (hard ↔ soft). Velocity → total energy.
- **Strike point as performance axis:** p modulatable per-note (MPE pressure/slide later; fixed-but-automatable parameter in Phase 1). Striking near eigenfunction nodal lines silences those modes — free, physically-coherent timbre control.
- **Bow (Phase 2+):** sustained per-mode noise/friction excitation gated by note-on; simple stochastic model first, refine by ear.

### 3.3 Damping
Damping is a design choice, not physics. Default: `decay_k = T60_global · (f_1/f_k)^tilt`, with `T60_global` and `tilt` as user params (tilt 0 = all modes equal, tilt 1 ≈ acoustic-plausible). Per-mode Q ceiling for stability.

### 3.4 Flow as modulation (Phase 2)
- Forward Ricci flow rate = "RELAX" knob (the geometric release envelope; spectrum migrates toward the topological attractor).
- Reverse/anti-flow = "SHARPEN" (curvature concentration → eigenfunction localization → emergent formant-like resonances; expressively risky, may need clamping).
- Local curvature injection at a point (press gesture) = transient localized resonance.
- Per-note flow restart vs. global continuous flow: **global flow shared across voices** is the more novel behavior (the *instrument itself* is evolving under your hands) — make it a mode switch, default global.

### 3.5 Voice architecture
- Phase 1–2: conventional polyphony — N voices (default 8), each its own modal bank state, all reading the *same* current spectrum frame (or per-note snapshot in "snapshot" mode).
- Phase 3 (stretch): surgery-driven voice splitting.

---

## 4. Architecture

### 4.1 Two-clock design

```
┌─────────────────────────── Geometry Thread (worker, ~50–100 Hz) ───────────────────────────┐
│  Mesh + metric state                                                                        │
│  → Ricci flow integrator (Chow–Luo, explicit Euler w/ adaptive dt; Newton later)            │
│  → Laplacian rebuild (cotan weights from updated edge lengths)                              │
│  → Eigen-update:                                                                            │
│       • full Lanczos re-solve every N frames (or on drift threshold)                        │
│       • first-order perturbation updates between solves: dλ_k = φ_kᵀ dL φ_k  (cheap)        │
│  → Emit SpectrumFrame { f[K], gain-coupling tables, damp[K], frame_id, timestamp }          │
└──────────────────────────────────────┬──────────────────────────────────────────────────────┘
                                       │  lock-free SPSC FIFO (preallocated ring of frames)
┌──────────────────────────────────────▼──────────────────────────────────────────────────────┐
│  Audio Thread (real-time)                                                                    │
│  → Pop latest SpectrumFrame (non-blocking; reuse previous if none)                           │
│  → Per-voice modal bank: K resonators (2-pole, coupled-form for stable coefficient sweeps)   │
│  → Coefficient smoothing: linear ramp over one control interval (no zipper)                  │
│  → Excitation injection from MIDI events (sample-accurate)                                   │
│  → Sum, soft-clip safety, out                                                                │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

**Hard real-time rules** (encode in CLAUDE.md):
- Zero allocation, locks, or system calls on the audio thread. All FIFO slots, voice state, and coefficient arrays preallocated in `prepareToPlay`.
- Eigensolves never block audio; if the geometry thread falls behind, audio coasts on the last frame (graceful, audible-as-stasis, never glitch).
- Coupled-form (a.k.a. magic-circle / rotation) resonators preferred over direct-form biquads: stable under continuous frequency sweeps, which is our whole modulation story.

### 4.2 Module breakdown

| Module | Responsibility | Phase |
|---|---|---|
| `geometry/Mesh` | Halfedge or index-based triangle mesh, edge lengths as primary metric state | 0–1 |
| `geometry/Laplacian` | Cotan stiffness + lumped mass from edge lengths (intrinsic — no vertex positions needed after init) | 0–1 |
| `geometry/EigenSolver` | Spectra shift-invert Lanczos wrapper; perturbation updater | 1 |
| `geometry/RicciFlow` | Chow–Luo circle-packing flow; curvature targets; reverse-flow clamps | 2 |
| `geometry/Surgery` | Pinch detection (degenerating radii), mesh split, spectrum handoff | 3 |
| `dsp/ModalBank` | K coupled-form resonators per voice, coefficient ramping | 1 |
| `dsp/Excitation` | Strike (eigenfunction-weighted impulse + mallet filter), bow | 1–2 |
| `engine/SpectrumBus` | Lock-free frame FIFO, frame interpolation policy | 1 |
| `engine/VoiceManager` | Allocation, stealing, snapshot-vs-global mode | 1 |
| `plugin/` | JUCE AudioProcessor, APVTS parameters, state save/load | 1 |
| `ui/` | Minimal generic UI Phase 1; manifold + spectrum visualizer Phase 4 | 1, 4 |
| `presets/manifolds/` | Icosphere (3 subdiv levels), torus (aspect-parametrized), genus-2 OBJ, user OBJ loader | 0–2 |

### 4.3 Performance budget (sanity check)
- Modal bank: 8 voices × 96 modes × ~10 flops/sample ≈ 7.7k flops/sample ≈ 0.37 GFLOPS @ 48 kHz — trivial.
- Geometry thread: mesh of 500–2000 vertices; Lanczos for 96 modes on a 2k×2k sparse matrix is tens of ms — fine at 1–5 Hz full-solve cadence with perturbation updates at 50–100 Hz between. Mesh resolution is the quality/latency knob; expose as a non-realtime "engine quality" setting.
- λ accuracy note: discrete spectra converge to continuum only for low modes on coarse meshes. We don't care about continuum fidelity — the *discrete object is the instrument* — but document this so genus-taxonomy tests compare against discrete analytic cases (icosphere vs. analytic sphere within tolerance bands, flat torus mesh vs. exact lattice).

---

## 5. Phased build plan (optimized for fastest audition)

Each phase = one Claude Code run (or `/loop` session) ending at a **CHECKPOINT** where Julian listens/plays and redirects. Oracles are automated; checkpoints are human.

### Phase 0 — Python audition rig (target: same-day sound)
**Goal: hear claims 1 and 2 before writing any C++.**
- `prototype/` Python package: load/generate meshes (icosphere, torus a:b, genus-2 OBJ), cotan Laplacian, `eigsh` lowest 96 modes, offline modal render to WAV (strike model, damping per §3.3).
- Crude flow: rather than full Ricci, Phase 0 may use *metric interpolation* between a start mesh and its conformally-uniformized target (precomputed) — enough to test correlated-drift perception. Full Chow–Luo in Python if time permits.
- Render matrix: {sphere, torus 1:1, torus 1:1.618, genus-2} × {3 strike points} × {static, flowing}. Plus the A/B for claim 2: flowing spectrum vs. decorrelated-drift control with matched marginal statistics.
- **Oracle:** FFT peaks of rendered WAVs match computed `c√λ_k` within 1 cent (static case); icosphere eigenvalues within tolerance of `l(l+1)` degeneracy structure; torus matches lattice formula exactly (flat metric is exact in discrete setting).
- **CHECKPOINT 0:** Julian auditions WAV matrix. Kill/pivot/proceed decision. Tune damping defaults by ear here — cheapest place to do it.

### Phase 1 — JUCE plugin skeleton, static geometry (first playable build)
- CMake + JUCE 8 project, VST3 + Standalone targets (Standalone app = fastest iteration; no DAW relaunch).
- Port Laplacian/eigensolver to Eigen/Spectra; load preset manifolds; full audio architecture from §4.1 *with the geometry thread running but flow disabled* (it just serves static frames — exercises the FIFO path from day one).
- MIDI in, 8 voices, params: manifold select, strike point, mallet, T60, tilt, mode count.
- **Oracle:** pluginval strictness 10 passes; C++ eigenvalues match Phase 0 Python within tolerance (libigl as cross-check); rendered-audio FFT oracle ported from Phase 0; address-sanitizer + thread-sanitizer clean on Standalone; no audio-thread allocations (JUCE `AudioProcessorValueTreeState` + manual audit, or use a malloc-canary in debug).
- **CHECKPOINT 1:** Julian plays it in Ableton Live 12. Latency, voice feel, strike-point behavior.

### Phase 2 — Ricci flow modulation (the actual thesis)
- Chow–Luo combinatorial Ricci flow on circle-packing metric; RELAX/SHARPEN params; global-vs-snapshot voice mode; perturbation-update fast path with periodic full re-solves; bow excitation.
- **Oracle:** flow converges: max |K_i − K̄| decreasing monotonically (forward flow) on all preset manifolds; eigenvalue trajectories continuous (no frame-to-frame jumps > threshold except at scheduled re-solves, which must be reconciled smoothly); long-run stability soak test (30 min flow, no NaN/denormal/divergence); reverse flow clamps respected.
- **CHECKPOINT 2:** The big one — does flow *sound like phrasing*? Expect significant by-ear retuning of flow-rate ranges and damping interaction.

### Phase 3 — Performance depth (order by Julian's Phase-2 feedback)
Candidates: local curvature injection (pressure gesture), MPE, strike-point-per-note, surgery/voice-splitting, OBJ import UX, modulation routing to Ableton (flow rate as automatable param is already free via APVTS).
Surgery oracle: pinch detection fires before numerical degeneracy; child meshes valid (Euler characteristic checks); spectrum handoff continuous in energy.

### Phase 4 — Visualization UI
- OpenGL manifold render with curvature heatmap + live spectrum overlay; strike point picking by clicking the surface. (High value for the consultancy-demo angle: the engine is *legible* — you watch the shape make the sound.)

---

## 6. Repo layout

```
curvsynth/
├── CLAUDE.md                  # RT rules, oracle commands, phase gates, "no audio-thread allocs" etc.
├── PROPOSAL.md                # this document
├── CMakeLists.txt
├── prototype/                 # Phase 0 Python rig (kept permanently as reference oracle)
│   ├── meshes.py  laplacian.py  render.py  oracles.py  audition_matrix.py
├── src/
│   ├── geometry/  dsp/  engine/  plugin/  ui/
├── assets/manifolds/          # icosphere_{1,2,3}.obj, torus presets, genus2.obj
├── tests/                     # Catch2: laplacian_test, eigen_oracle_test, flow_convergence_test, rt_safety_test
└── tools/
    ├── render_offline.cpp     # headless WAV render sharing src/ DSP — keeps spectral oracle running post-Python
    └── run_pluginval.sh
```

**CLAUDE.md must encode:** build/test commands; the audio-thread prohibition list; "all DSP changes must keep `tools/render_offline` building so oracles never go dark"; checkpoint protocol (stop, summarize state, produce audition artifacts, wait).

---

## 7. Risks & mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Correlated-drift coherence claim fails perceptually | Medium | Phase 0 tests it for ~$0; pivot options: slower flow regimes, mode-count changes, or reframe engine around static exotic spectra + strike-point performance (still a viable instrument) |
| Chow–Luo numerics fiddly (obtuse triangles, packing degeneracy) | Medium | Start with inversive-distance circle packing (Jin et al.) or fall back to CETM conformal factors; mesh-quality preprocessing (isotropic remesh offline) |
| Eigen-solve cadence too slow → audible stasis between updates | Low | Perturbation fast path; reduce mesh res; interpolate frames in SpectrumBus |
| Reverse flow blows up (it's the ill-posed direction) | High (by design) | Hard clamps on curvature concentration + flow-time limits; treat instability as bounded expressive territory, not open territory |
| Resonator instability under fast sweeps | Low | Coupled-form resonators + per-sample coefficient ramps; soft-clip safety net |
| Surgery (Phase 3) is a research project in itself | High | It's a stretch goal; engine is complete without it |
| JUCE/Ableton integration friction (param automation, state restore) | Low | Standard APVTS patterns; pluginval + manual Live session test in every checkpoint |

---

## 8. Open decisions for Julian before Run 1

1. **Platform target:** macOS-first (AU also nearly free under JUCE) or macOS+Windows from day one? CMake is cross-platform either way; CI scope differs.
2. **Mode count default** (96 proposed) and max mesh resolution — quality vs. flow-latency tradeoff.
3. **Phase 0 scope:** pure metric-interpolation flow (fastest) or insist on real Chow–Luo in Python (1 extra step, truer test of claim 3)?
4. **MPE ambitions:** wire MPE plumbing in Phase 1 (cheap then, annoying later) even though gestures land in Phase 3?
5. **License posture:** JUCE GPL (open-source the engine — strong consultancy-demo artifact) vs. commercial license path.
