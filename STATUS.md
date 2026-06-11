# Project status

**Phase 3 (run 1) — COMPLETE (2026-06-11). Holding at CHECKPOINT 3.**

Checkpoint history: CHECKPOINT 0 passed (genus taxonomy + drift coherence;
aesthetic direction = implausible/"metaphysical" objects). CHECKPOINT 1
passed ("It's working!"). CHECKPOINT 2 passed ("working properly" — and the
in-chat flow visualizer was "extremely helpful", which set this run's
priorities).

## Phase 3 run 1: legibility + local gesture

- **In-plugin visualizer** (Phase 4 pulled forward on Julian's feedback):
  custom editor replaces the generic one. Left: manifold with live curvature
  heatmap (coral above target / teal below), conformal bulge, drag-to-rotate,
  **click the surface to set the strike point**. Right: scrolling partial-
  ratio traces + curvature error readout + full parameter panel. 30 Hz
  software rendering; geometry state arrives via a second wait-free triple
  buffer (`VizBus`), editor/message thread is sole consumer.
- **Press** param: continuous localized curvature injection at the strike
  vertex (BFS-bump profile, clamps apply) — a localized SHARPEN that grows a
  resonant deformation under your finger; Relax heals it. Visible live in
  the editor.
- **Damp Comb** param: every other mode's T60 collapses — the selective-
  absorption implausible material from checkpoint 0, now performable.
- **Pitch bend**: +/-2 semitones, whole spectrum rigidly.

## Phase 3 run 1 gates — all pass

- Catch2 suite (14 cases; new: press localization + heal-by-relax): pass
- pluginval strictness 10 (includes editor open/close): SUCCESS
- C++ FFT oracle: 14 partials within 1 cent (worst 0.000)
- ASan+UBSan suite: clean

## What's new in Phase 2

- **Chow–Luo combinatorial Ricci flow** (`src/geometry/RicciFlow.*`,
  prototyped + validated in `prototype/ricci.py` first): inversive-distance
  circle packing, explicit Euler with step-halving on metric validity and
  (forward) on curvature-error monotonicity. Reverse flow (SHARPEN) runs the
  field backwards under hard |u−u0| clamps — bounded expressive territory.
- **Perturbation fast path:** Rayleigh quotients of the stale eigenbasis
  with the current operator at every flow step (~25 ms); scheduled full
  re-solves every 0.6 units of accumulated flow time, mode-matched by
  eigenvector overlap × eigenvalue proximity, then **blended over 8 frames**
  so a re-solve never lands as an audible step.
- **Plugin params:** Flow (Off/Relax/Sharpen), Flow Rate, Kick (each toggle
  = one conformal perturbation), Voices (Snapshot/Global Flow, default
  Global — the instrument itself evolves under your hands), Bow.
- **Bow excitation:** per-control-interval stochastic energy injection
  weighted by the strike coupling pattern ("simple stochastic model first" —
  refine by ear).
- `render_offline` gained `--flow/--flow-rate/--kick/--bow` so the offline
  oracle path exercises the full flow machinery.

## Phase 2 gates — all pass

- Forward flow: max |K_i − K̄| monotonically decreasing on **all five
  presets** (Python oracle: genus 0/1/2; C++ test: icosphere + golden torus)
- Eigenvalue trajectories continuous: ≤5% frame-to-frame ratio change
  through fast-path updates AND scheduled re-solves (Catch2 test)
- **30-min soak × 2** (golden torus and genus-2; 72k steps each, alternating
  Relax/Sharpen/kick): no NaN/denormal/divergence
- Reverse-flow clamps respected (Python oracle + C++ test)
- pluginval strictness 10: SUCCESS (re-run post-Phase 2)
- C++ FFT oracle: 14 partials within 1 cent (worst 0.000)
- ASan+UBSan: full suite clean. TSan: SPSC bus stress clean (threading
  pattern unchanged from Phase 1: single geometry producer, audio consumer).

## Tuning notes for the checkpoint (everything by-ear is provisional)

- Flow Rate maps quadratically to dt (max 0.25/step at 25 ms cadence).
- Kick amplitude fixed at 0.6 conformal units; geometry-thread seed advances
  per kick so each one is a different bump pattern.
- Bow gain scales with amount²; injection at 750 Hz control rate.

## CHECKPOINT 3 — what Julian checks (in Live)

1. The editor: does the manifold view make the instrument legible while
   playing? Click-to-strike workflow; rotate during flow.
2. Press as a gesture (automate it or ride it): localized bump -> localized
   spectral change, healed by Relax.
3. Damp Comb sweep on genus-2; pitch bend feel.

## Remaining Phase 3 candidates (next runs, order by feedback)

MPE, strike-point-per-note, metric-morph targets as a parameter,
surgery/voice-splitting (stretch), OBJ import UX. Phase 4 polish: nicer
shading, spectrum labels in Hz, curvature-injection by clicking with a
modifier key.
