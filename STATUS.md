# Project status

**Phase 2 — COMPLETE (2026-06-10). Holding at CHECKPOINT 2: the big one.**

The question on the table: **does flow sound like phrasing?** Play the plugin
in Live — Kick + Relax while holding a chord, Sharpen mid-phrase, Flow Rate
as a performance control. Offline previews: `renders/checkpoint2/`.

Checkpoint history: CHECKPOINT 0 passed (genus taxonomy + drift coherence by
ear; aesthetic direction = implausible/"metaphysical" objects, push beyond
mild inversions). CHECKPOINT 1 passed ("It's working!" — loads and plays in
Live 12.4.5b3).

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

## Next (pending checkpoint verdict)

Phase 3 candidates, ordered by Julian's Phase-2 feedback: local curvature
injection (pressure gesture), MPE, strike-point-per-note, surgery/voice
splitting, OBJ import, deeper implausibility features (metric morph targets,
per-mode damping masks as params). Phase 4: visualization UI.
